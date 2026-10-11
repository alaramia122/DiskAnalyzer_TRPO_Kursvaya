#include "application/ApplicationController.h"
#include "infrastructure/aggregator/DirectoryAggregator.h"
#include "infrastructure/classifier/FileClassifier.h"
#include "infrastructure/scanner/DirectoryScanner.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

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

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "DiskAnalyzer_ControllerTest";
    std::error_code fileSystemError;
    fs::remove_all(root, fileSystemError);
    fs::create_directories(root);

    // Набор файлов делает отмену второго анализа воспроизводимой:
    // запрос отмены отправляется сразу после старта фонового потока.
    for (int index = 0; index < 2000; ++index)
    {
        const fs::path filePath = root / ("file-" + std::to_string(index) + ".txt");
        std::ofstream file(filePath, std::ios::binary);
        file << "controller test";
    }

    infrastructure::FileClassifier classifier;
    infrastructure::DirectoryScanner scanner(classifier);
    infrastructure::DirectoryAggregator aggregator;
    application::AnalysisPipeline pipeline(scanner, aggregator);
    application::ApplicationController controller(pipeline);

    bool passed = true;
    int finishedCount = 0;
    int firstStatus = -1;
    int secondStatus = -1;
    QEventLoop eventLoop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&timeout, &QTimer::timeout, &eventLoop, [&eventLoop]()
    {
        std::cerr << "FAILED: controller test timed out.\n";
        eventLoop.quit();
    });

    QObject::connect(
        &controller,
        &application::ApplicationController::analysisFinished,
        &eventLoop,
        [&](int status)
        {
            ++finishedCount;
            if (finishedCount == 1)
            {
                firstStatus = status;
                passed &= require(
                    !controller.startAnalysis(root),
                    "controller rejects a new analysis during completion notification");
                QTimer::singleShot(0, &eventLoop, [&]()
                {
                    passed &= require(
                        controller.startAnalysis(root),
                        "controller allows a new analysis after completion callback");
                    controller.cancelAnalysis();
                });
            }
            else if (finishedCount == 2)
            {
                secondStatus = status;
                eventLoop.quit();
            }
        });

    passed &= require(
        controller.startAnalysis(root),
        "controller starts the first analysis");
    passed &= require(
        !controller.startAnalysis(root),
        "controller rejects a second analysis while the first is active");

    timeout.start(15000);
    eventLoop.exec();
    controller.waitForFinished();

    passed &= require(
        finishedCount == 2,
        "controller emits completion for both analysis runs");
    passed &= require(
        firstStatus == static_cast<int>(domain::ScanStatus::Completed),
        "first controller run completes");
    passed &= require(
        secondStatus == static_cast<int>(domain::ScanStatus::Cancelled),
        "second controller run can be cancelled");
    passed &= require(
        !controller.isRunning(),
        "controller reports not running after completion");

    fs::remove_all(root, fileSystemError);

    if (!passed)
    {
        return 1;
    }

    std::cout << "All controller checks passed.\n";
    return 0;
}
