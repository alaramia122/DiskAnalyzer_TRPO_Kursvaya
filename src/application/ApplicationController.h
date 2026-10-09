#pragma once

#include "AnalysisPipeline.h"
#include "../domain/models/AnalysisResult.h"

#include <QObject>
#include <QString>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

namespace application
{
    /**
     * @brief Управляет жизненным циклом фонового анализа.
     *
     * ApplicationController относится к application layer и может зависеть
     * от Qt. Domain и инфраструктурные модели от Qt не зависят.
     *
     * Сигналы испускаются в потоке QObject: сообщения из worker-потока
     * перенаправляются через QMetaObject::invokeMethod с QueuedConnection.
     */
    class ApplicationController final : public QObject
    {
        Q_OBJECT

    public:
        ApplicationController(
            const AnalysisPipeline& pipeline,
            QObject* parent = nullptr);

        ~ApplicationController() override;

        ApplicationController(const ApplicationController&) = delete;
        ApplicationController& operator=(const ApplicationController&) = delete;

        /**
         * @brief Запускает анализ в отдельном потоке.
         * @return false, если предыдущая задача еще не присоединена или идет.
         */
        bool startAnalysis(const std::filesystem::path& rootPath);

        /**
         * @brief Запрашивает отмену текущего анализа.
         */
        void cancelAnalysis();

        /**
         * @brief Ожидает завершения фонового потока.
         */
        void waitForFinished();

        /**
         * @brief Возвращает результат. Вызывать после сигнала analysisFinished.
         */
        const domain::AnalysisResult& result() const;

        bool isRunning() const;

    signals:
        void analysisStarted();
        void progressChanged(
            QString currentPath,
            qulonglong processedFiles,
            qulonglong processedDirectories);
        void scanError(QString path, QString message);
        void analysisFinished(int status);

    private:
        const AnalysisPipeline& pipeline_;
        domain::AnalysisResult result_;
        std::atomic_bool cancellationRequested_{false};
        std::atomic_bool running_{false};
        std::atomic_bool completionPending_{false};
        std::thread worker_;
        mutable std::mutex lifecycleMutex_;
    };
}
