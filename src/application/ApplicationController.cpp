#include "ApplicationController.h"

#include <QMetaObject>
#include <QString>

#include <system_error>
#include <utility>

namespace application
{
    ApplicationController::ApplicationController(
        const AnalysisPipeline& pipeline,
        QObject* parent)
        : QObject(parent),
          pipeline_(pipeline)
    {
    }

    ApplicationController::~ApplicationController()
    {
        cancelAnalysis();
        waitForFinished();
    }

    bool ApplicationController::startAnalysis(
        const std::filesystem::path& rootPath)
    {
        std::lock_guard<std::mutex> lock(lifecycleMutex_);

        if (running_.load())
        {
            return false;
        }

        // Завершившийся поток присоединяется перед повторным запуском.
        if (worker_.joinable())
        {
            worker_.join();
        }

        cancellationRequested_.store(false);
        running_.store(true);

        worker_ = std::thread([this, rootPath]()
        {
            QMetaObject::invokeMethod(
                this,
                [this]()
                {
                    emit analysisStarted();
                },
                Qt::QueuedConnection);

            const domain::ScanStatus status = pipeline_.run(
                rootPath,
                result_,
                cancellationRequested_,
                [this](const domain::ScanProgress& progress)
                {
                    const QString path =
                        QString::fromStdString(progress.currentPath.string());

                    QMetaObject::invokeMethod(
                        this,
                        [this, path,
                         files = static_cast<qulonglong>(progress.processedFiles),
                         directories = static_cast<qulonglong>(
                             progress.processedDirectories)]()
                        {
                            emit progressChanged(path, files, directories);
                        },
                        Qt::QueuedConnection);
                },
                [this](const domain::ScanError& error)
                {
                    const QString path =
                        QString::fromStdString(error.path.string());
                    const QString message =
                        QString::fromStdString(error.message);

                    QMetaObject::invokeMethod(
                        this,
                        [this, path, message]()
                        {
                            emit scanError(path, message);
                        },
                        Qt::QueuedConnection);
                });

            running_.store(false);

            QMetaObject::invokeMethod(
                this,
                [this, status]()
                {
                    emit analysisFinished(static_cast<int>(status));
                },
                Qt::QueuedConnection);
        });

        return true;
    }

    void ApplicationController::cancelAnalysis()
    {
        cancellationRequested_.store(true);
    }

    void ApplicationController::waitForFinished()
    {
        std::lock_guard<std::mutex> lock(lifecycleMutex_);

        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    const domain::AnalysisResult& ApplicationController::result() const
    {
        return result_;
    }

    bool ApplicationController::isRunning() const
    {
        return running_.load();
    }
}
