#include "infrastructure/export/ExportManager.h"
#include "infrastructure/aggregator/DirectoryAggregator.h"
#include "infrastructure/classifier/FileClassifier.h"
#include "infrastructure/scanner/DirectoryScanner.h"
#include "application/AnalysisPipeline.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
    bool require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\\n';
            return false;
        }
        return true;
    }

    std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
    }

    std::string jsonEscapedPath(const std::filesystem::path& path)
    {
        std::string escaped = path.u8string();
        std::string result;
        result.reserve(escaped.size());
        for (const char character : escaped)
        {
            if (character == '\\\\')
            {
                result += "\\\\\\\\";
            }
            else
            {
                result += character;
            }
        }
        return result;
    }
}

int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "DiskAnalyzer_ExportTest";
    const fs::path jsonPath = root / "result.json";
    const fs::path txtPath = root / "result.txt";

    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "nested");

    {
        std::ofstream file(root / "example.txt", std::ios::binary);
        file << "data";
    }

    infrastructure::FileClassifier classifier;
    infrastructure::DirectoryScanner scanner(classifier);
    infrastructure::DirectoryAggregator aggregator;
    application::AnalysisPipeline pipeline(scanner, aggregator);
    domain::AnalysisResult result;
    std::atomic_bool cancelRequested(false);

    bool passed = true;
    passed &= require(
        pipeline.run(root, result, cancelRequested) == domain::ScanStatus::Completed,
        "fixture analysis completes");

    infrastructure::ExportManager exporter;
    std::string errorMessage;
    passed &= require(
        exporter.exportResult(result, jsonPath, domain::ExportFormat::Json, &errorMessage),
        "JSON export succeeds");
    const std::string json = readFile(jsonPath);
    passed &= require(json.find("\\\"root\\\"") != std::string::npos,
                      "JSON contains root object");
    passed &= require(json.find("\\\"filesCount\\\": 1") != std::string::npos,
                      "JSON contains direct file count");
    passed &= require(json.find(jsonEscapedPath(root)) != std::string::npos,
                      "JSON contains root directory path");

    passed &= require(
        exporter.exportResult(result, txtPath, domain::ExportFormat::Txt, &errorMessage),
        "TXT export succeeds");
    const std::string txt = readFile(txtPath);
    passed &= require(txt.find("Результаты анализа") != std::string::npos,
                      "TXT contains human-readable heading");
    passed &= require(txt.find(root.u8string()) != std::string::npos,
                      "TXT contains root directory path");

    const fs::path missingOutput = root / "missing-parent" / "result.json";
    passed &= require(
        !exporter.exportResult(result, missingOutput, domain::ExportFormat::Json, &errorMessage),
        "export reports output-open failure");
    passed &= require(!errorMessage.empty(),
                      "export reports a useful error message");

    // An invalid format must not truncate an existing destination file.
    const fs::path invalidFormatPath = root / "preserve.txt";
    {
        std::ofstream existing(invalidFormatPath, std::ios::binary);
        existing << "preserve this content";
    }
    passed &= require(
        !exporter.exportResult(
            result, invalidFormatPath,
            static_cast<domain::ExportFormat>(999), &errorMessage),
        "export rejects an unsupported format");
    passed &= require(
        !errorMessage.empty(),
        "unsupported format reports an error");
    passed &= require(
        readFile(invalidFormatPath) == "preserve this content",
        "unsupported format does not truncate an existing file");

    domain::AnalysisResult emptyResult;
    passed &= require(
        !exporter.exportResult(emptyResult, jsonPath, domain::ExportFormat::Json, &errorMessage),
        "export refuses an empty result");

    fs::remove_all(root, error);

    if (!passed)
    {
        return 1;
    }

    std::cout << "All export checks passed.\\n";
    return 0;
}
