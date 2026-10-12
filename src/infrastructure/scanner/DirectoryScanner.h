#pragma once

#include "../../domain/interfaces/IDirectoryScanner.h"
#include "../../domain/interfaces/IFileClassifier.h"

namespace infrastructure
{
    /**
     * @brief Реализация рекурсивного сканирования файловой системы.
     *
     * Не хранит FileInfo после обработки файла.
     * Не вычисляет subtree-статистику.
     * Не следует по symbolic links в дочерние каталоги.
     */
    class DirectoryScanner final : public domain::IDirectoryScanner
    {
    public:
        /**
         * @brief Создает Scanner с указанным классификатором файлов.
         * @param classifier Реализация классификатора файлов.
         */
        explicit DirectoryScanner(
            const domain::IFileClassifier& classifier);

        domain::ScanStatus scan(
            const std::filesystem::path& rootPath,
            domain::AnalysisResult& result,
            const std::atomic_bool& cancellationRequested,
            const domain::ScanProgressCallback& progressCallback = {},
            const domain::ScanErrorCallback& errorCallback = {}) const override;

    private:
        static constexpr std::uint64_t ProgressInterval = 256;

        const domain::IFileClassifier& classifier_;
    };
}
