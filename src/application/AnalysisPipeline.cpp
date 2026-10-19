#include "AnalysisPipeline.h"

namespace application
{
    // Сохраняем переданные зависимости по ссылке: их жизненный цикл должен
    // быть длиннее жизненного цикла конвейера.
    AnalysisPipeline::AnalysisPipeline(
        const domain::IDirectoryScanner& scanner,
        const domain::IDirectoryAggregator& aggregator)
        : scanner_(scanner),
          aggregator_(aggregator)
    {
    }

    // Конвейер задаёт порядок операций и передаёт callbacks сканеру.
    // Агрегация запускается только при статусе Completed. Пропуски отдельных
    // объектов при этом отражаются отдельно через DirectoryNode::scanComplete.
    domain::ScanStatus AnalysisPipeline::run(
        const std::filesystem::path& rootPath,
        domain::AnalysisResult& result,
        const std::atomic_bool& cancellationRequested,
        const domain::ScanProgressCallback& progressCallback,
        const domain::ScanErrorCallback& errorCallback) const
    {
        const domain::ScanStatus scanStatus = scanner_.scan(
            rootPath,
            result,
            cancellationRequested,
            progressCallback,
            errorCallback);

        // При неуспешном или отменённом сканировании дерево нельзя считать
        // полностью подготовленным для агрегации.
        if (scanStatus != domain::ScanStatus::Completed)
        {
            return scanStatus;
        }

        // Отмена могла поступить сразу после завершения сканирования.
        if (cancellationRequested.load())
        {
            return domain::ScanStatus::Cancelled;
        }

        // Агрегатор вычисляет сводную статистику поддеревьев поверх direct-данных.
        if (!aggregator_.aggregate(result))
        {
            return domain::ScanStatus::Failed;
        }

        // Проверяем запрос ещё раз перед выдачей итогового статуса.
        if (cancellationRequested.load())
        {
            return domain::ScanStatus::Cancelled;
        }

        return domain::ScanStatus::Completed;
    }
}
