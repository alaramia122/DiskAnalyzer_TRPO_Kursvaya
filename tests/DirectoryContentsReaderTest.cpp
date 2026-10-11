#include "infrastructure/classifier/FileClassifier.h"
#include "infrastructure/reader/DirectoryContentsReader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    bool require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("disk-analyzer-reader-test-" + std::to_string(stamp));
    const fs::path nested = root / "nested";
    std::error_code error;
    fs::create_directories(nested, error);
    if (error)
    {
        std::cerr << "Could not create test directory: " << error.message() << '\n';
        return 1;
    }

    {
        std::ofstream file(root / "note.txt");
        file << "abc";
    }
    {
        std::ofstream file(nested / "hidden-from-recursion.cpp");
        file << "int main() {}";
    }

    infrastructure::FileClassifier classifier;
    infrastructure::DirectoryContentsReader reader(classifier);
    const auto result = reader.read(root);

    bool passed = true;
    passed &= require(result.opened, "existing directory is reported as opened");
    passed &= require(result.errors.empty(), "normal directory read has no errors");
    passed &= require(result.entries.size() == 2,
                      "reader returns only immediate entries");
    bool foundText = false;
    bool foundNestedDirectory = false;
    for (const auto& entry : result.entries)
    {
        if (entry.path.filename() == "note.txt")
        {
            foundText = entry.type == domain::FileSystemObjectType::File &&
                        entry.sizeBytes == 3 &&
                        entry.category == domain::FileCategory::Documents;
        }
        if (entry.path.filename() == "nested")
        {
            foundNestedDirectory =
                entry.type == domain::FileSystemObjectType::Directory;
        }
    }
    passed &= require(foundText, "regular file metadata and category are populated");
    passed &= require(foundNestedDirectory, "subdirectory is listed without recursion");

    // Проверяем символические ссылки там, где ОС и права пользователя
    // позволяют их создавать. На Windows создание ссылок может быть запрещено
    // политикой системы, поэтому отказ создания не является падением теста.
    const fs::path directoryLink = root / "nested-link";
    std::error_code symlinkError;
    fs::create_directory_symlink(nested, directoryLink, symlinkError);
    if (!symlinkError)
    {
        const auto withDirectoryLink = reader.read(root);
        bool foundDirectoryLink = false;
        for (const auto& entry : withDirectoryLink.entries)
        {
            if (entry.path.filename() == "nested-link")
            {
                foundDirectoryLink =
                    entry.type == domain::FileSystemObjectType::Directory;
            }
        }
        passed &= require(foundDirectoryLink,
                          "symlink to a directory is shown as a directory");
    }

    const fs::path brokenLink = root / "broken-link";
    symlinkError.clear();
    fs::create_symlink(root / "target-that-does-not-exist",
                       brokenLink, symlinkError);
    if (!symlinkError)
    {
        const auto withBrokenLink = reader.read(root);
        bool reportedBrokenLink = false;
        for (const auto& readError : withBrokenLink.errors)
        {
            if (readError.path.filename() == "broken-link")
            {
                reportedBrokenLink = true;
            }
        }
        passed &= require(reportedBrokenLink,
                          "broken symlink is reported as a read error");
    }

    const auto missing = reader.read(root / "does-not-exist");
    passed &= require(!missing.opened, "missing directory is not reported as opened");
    passed &= require(!missing.errors.empty(),
                      "opening a missing directory reports an error");

    fs::remove_all(root, error);
    if (!passed)
    {
        return 1;
    }

    std::cout << "All directory reader checks passed.\n";
    return 0;
}
