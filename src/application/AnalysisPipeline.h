#pragma once

#include "../domain/interfaces/IDirectoryAggregator.h"
#include "../domain/interfaces/IDirectoryScanner.h"

namespace application
{
    /**
     * @brief Координатор последовательности сканирования и агрегации.
     *
     * Не создает потоки и не зависит от Qt. Асинхронный запуск и управление
     * жизненным циклом задачи относятся к ApplicationController.
     */
    class AnalysisPipeline final
    {
    public:
        /**
         * @brief Создает конвейер с внедренными зависимостями.
         * @param scanner Реализация сканера файловой системы.
         * @param aggregator Реализация агрегатора статистики.
         */
        AnalysisPipeline(
            const domain::IDirectoryScanner& scanner,
            const domain::IDirectoryAggregator& aggregator);

        /**
         * @brief Выполняет сканирование и агрегирование результата.
         * @param rootPath Корневой каталог.
         * @param result Объект результата анализа.
         * @param cancellationRequested Флаг отмены.
         * @param progressCallback Уведомление о прогрессе сканирования.
         * @param errorCallback Уведомление об ошибках файловой системы.
         * @return Итоговый статус операции.
         */
        domain::ScanStatus run(
            const std::filesystem::path& rootPath,
            domain::AnalysisResult& result,
            const std::atomic_bool& cancellationRequested,
            const domain::ScanProgressCallback& progressCallback = {},
            const domain::ScanErrorCallback& errorCallback = {}) const;

    private:
        const domain::IDirectoryScanner& scanner_;
        const domain::IDirectoryAggregator& aggregator_;
    };
}
