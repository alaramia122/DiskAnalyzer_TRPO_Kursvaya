#include "application/AnalysisPipeline.h"
#include "infrastructure/aggregator/DirectoryAggregator.h"
#include "infrastructure/classifier/FileClassifier.h"
#include "infrastructure/scanner/DirectoryScanner.h"

#include <atomic>
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

    const fs::path testRoot =
        fs::temp_directory_path() / "DiskAnalyzer_TRPO_IntegrationTest";

    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);

    fs::create_directories(testRoot / "nested");
    fs::create_directories(testRoot / "empty");

    // Создание ссылок поддерживается не на всех окружениях/правах.
    // Если ОС позволяет создать directory symlink, проверяем его политику.
    const fs::path directorySymlink = testRoot / "nested-link";
    std::error_code symlinkCreationError;
    fs::create_directory_symlink(testRoot / "nested", directorySymlink,
                                 symlinkCreationError);
    const bool directorySymlinkCreated = !symlinkCreationError;

    {
        std::ofstream rootFile(testRoot / "root.txt", std::ios::binary);
        rootFile << "abc";
    }

    {
        std::ofstream nestedFile(
            testRoot / "nested" / "source.cpp",
            std::ios::binary);
        nestedFile << "12345";
    }

    infrastructure::FileClassifier classifier;
    infrastructure::DirectoryScanner scanner(classifier);
    infrastructure::DirectoryAggregator aggregator;
    application::AnalysisPipeline pipeline(scanner, aggregator);

    domain::AnalysisResult result;
    std::atomic_bool cancellationRequested(false);

    const domain::ScanStatus status = pipeline.run(
        testRoot,
        result,
        cancellationRequested);

    bool passed = true;
    passed &= require(
        status == domain::ScanStatus::Completed,
        "pipeline completes for a readable directory");
    passed &= require(
        result.getRoot() != nullptr,
        "result contains a root node");

    if (result.getRoot())
    {
        const domain::DirectoryStatistics& stats = result.getRoot()->stats;

        passed &= require(
            stats.direct.filesCount == 1,
            "root direct file count is one");
        passed &= require(
            stats.direct.filesSizeBytes == 3,
            "root direct size is three bytes");
        passed &= require(
            stats.subtree.filesCount == 1,
            "root subtree file count includes nested files");
        passed &= require(
            stats.subtree.filesSizeBytes == 5,
            "root subtree size includes nested file size");
        passed &= require(
            stats.getTotalCategoryCount(domain::FileCategory::Documents) == 1,
            "txt file is classified as Documents");
        passed &= require(
            stats.getTotalCategoryCount(domain::FileCategory::SourceCode) == 1,
            "cpp file is classified as SourceCode");

        const domain::DirectoryNode* emptyDirectory =
            result.findNode(testRoot / "empty");
        passed &= require(
            emptyDirectory != nullptr,
            "empty directory is present in the result tree");
        if (emptyDirectory)
        {
            passed &= require(
                emptyDirectory->stats.direct.filesCount == 0 &&
                emptyDirectory->stats.subtree.filesCount == 0,
                "empty directory has zero direct and subtree file counts");
            passed &= require(
                emptyDirectory->children.empty(),
                "empty directory has no child directories");
        }

        if (directorySymlinkCreated)
        {
            const domain::DirectoryNode* linkNode =
                result.findNode(directorySymlink);
            passed &= require(
                linkNode != nullptr,
                "directory symlink is represented as a directory node");
            if (linkNode)
            {
                passed &= require(
                    linkNode->children.empty(),
                    "directory symlink is not recursively traversed");
                passed &= require(
                    linkNode->stats.direct.filesCount == 0 &&
                    linkNode->stats.subtree.filesCount == 0,
                    "directory symlink target files are not counted twice");
            }
        }
    }

    // Повторная агрегация не должна повторно прибавлять subtree-статистику.
    passed &= require(
        aggregator.aggregate(result),
        "aggregator accepts a populated result on repeated call");

    if (result.getRoot())
    {
        passed &= require(
            result.getRoot()->stats.subtree.filesCount == 1,
            "repeated aggregation does not duplicate subtree file count");
        passed &= require(
            result.getRoot()->stats.subtree.filesSizeBytes == 5,
            "repeated aggregation does not duplicate subtree file size");
    }

    domain::AnalysisResult cancelledResult;
    std::atomic_bool alreadyCancelled(true);
    const domain::ScanStatus cancelledStatus = pipeline.run(
        testRoot,
        cancelledResult,
        alreadyCancelled);

    passed &= require(
        cancelledStatus == domain::ScanStatus::Cancelled,
        "pipeline returns Cancelled when cancellation is requested");

    // Отмена из callback прогресса проверяет остановку уже запущенного обхода,
    // а не только обработку флага, установленного до вызова pipeline.
    domain::AnalysisResult midScanCancelledResult;
    std::atomic_bool midScanCancellationRequested(false);
    bool initialProgressReported = false;
    const domain::ScanStatus midScanCancelledStatus = pipeline.run(
        testRoot,
        midScanCancelledResult,
        midScanCancellationRequested,
        [&](const domain::ScanProgress&)
        {
            if (!initialProgressReported)
            {
                initialProgressReported = true;
                midScanCancellationRequested.store(true);
            }
        });

    passed &= require(
        initialProgressReported,
        "pipeline reports initial progress before scanning entries");
    passed &= require(
        midScanCancelledStatus == domain::ScanStatus::Cancelled,
        "pipeline stops when cancellation is requested by progress callback");

    domain::AnalysisResult fileAsRootResult;
    const domain::ScanStatus fileAsRootStatus = pipeline.run(
        testRoot / "root.txt",
        fileAsRootResult,
        notCancelled);

    passed &= require(
        fileAsRootStatus == domain::ScanStatus::Failed,
        "pipeline rejects a regular file as the root directory");

    domain::AnalysisResult missingResult;
    std::atomic_bool notCancelled(false);
    const domain::ScanStatus missingPathStatus = pipeline.run(
        testRoot / "does-not-exist",
        missingResult,
        notCancelled);

    passed &= require(
        missingPathStatus == domain::ScanStatus::Failed,
        "pipeline fails for a nonexistent root path");

    fs::remove_all(testRoot, cleanupError);

    if (!passed)
    {
        return 1;
    }

    std::cout << "All integration checks passed.\n";
    return 0;
}
