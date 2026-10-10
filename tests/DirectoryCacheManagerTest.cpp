#include "infrastructure/cache/DirectoryCacheManager.h"

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

    domain::FileInfo makeFile(const std::filesystem::path& path)
    {
        domain::FileInfo info;
        info.path = path;
        info.sizeBytes = 12;
        info.category = domain::FileCategory::Documents;
        info.type = domain::FileSystemObjectType::File;
        return info;
    }
}

int main()
{
    namespace fs = std::filesystem;
    infrastructure::DirectoryCacheManager cache(2);
    const fs::path first = fs::path("root") / "first";
    const fs::path second = fs::path("root") / "second";
    const fs::path third = fs::path("root") / "third";

    bool passed = true;
    passed &= require(cache.store(first, {makeFile(first / "a.txt")}),
                      "cache accepts first directory");
    passed &= require(cache.store(second, {makeFile(second / "b.txt"),
                                           makeFile(second / "c.txt")}),
                      "cache accepts second directory");
    passed &= require(cache.cachedDirectoryCount() == 2 &&
                      cache.cachedFileCount() == 3,
                      "cache reports directory and file counts");

    passed &= require(cache.find(first) != nullptr,
                      "cached directory can be retrieved");
    cache.store(third, {makeFile(third / "d.txt")});
    passed &= require(cache.find(first) != nullptr,
                      "recently used directory remains cached");
    passed &= require(cache.find(second) == nullptr,
                      "least recently used directory is evicted");

    cache.setCurrentDirectory(third);
    passed &= require(cache.currentDirectory() == third,
                      "current directory is tracked independently");
    cache.clear();
    passed &= require(cache.cachedDirectoryCount() == 0 &&
                      cache.cachedFileCount() == 0 &&
                      cache.currentDirectory().empty(),
                      "clear releases cached entries and current path");

    infrastructure::DirectoryCacheManager disabled(0);
    passed &= require(!disabled.store(first, {makeFile(first / "x.txt")}),
                      "zero-capacity cache rejects storage");

    if (!passed)
    {
        return 1;
    }

    std::cout << "All directory cache checks passed.\n";
    return 0;
}
