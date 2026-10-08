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

    fs::remove_all(testRoot, cleanupError);

    if (!passed)
    {
        return 1;
    }

    std::cout << "All integration checks passed.\n";
    return 0;
}
