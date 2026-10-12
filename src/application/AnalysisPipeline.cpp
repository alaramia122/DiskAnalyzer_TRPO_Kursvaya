#include "AnalysisPipeline.h"

namespace application
{
    AnalysisPipeline::AnalysisPipeline(
        const domain::IDirectoryScanner& scanner,
        const domain::IDirectoryAggregator& aggregator)
        : scanner_(scanner),
          aggregator_(aggregator)
    {
    }

    // Конвейер задаёт порядок операций и передаёт callbacks сканеру.
    // Агрегация выполняется только после полного обхода; при отмене или
    // ошибке сканирования частичный результат не выдаётся за завершённый.
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

        if (scanStatus != domain::ScanStatus::Completed)
        {
            return scanStatus;
        }

        if (cancellationRequested.load())
        {
            return domain::ScanStatus::Cancelled;
        }

        if (!aggregator_.aggregate(result))
        {
            return domain::ScanStatus::Failed;
        }

        if (cancellationRequested.load())
        {
            return domain::ScanStatus::Cancelled;
        }

        return domain::ScanStatus::Completed;
    }
}
