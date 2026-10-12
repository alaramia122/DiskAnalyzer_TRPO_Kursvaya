#pragma once

#include "../models/AnalysisResult.h"
#include "../enums/ScanStatus.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>

namespace domain
{
    /**
     * @brief Ошибка, возникшая при обходе файловой системы.
     *
     * Ошибка отдельного объекта не обязана останавливать весь анализ.
     * Решение о продолжении принимает реализация Scanner в соответствии
     * с правилами обработки ошибок.
     */
    struct ScanError
    {
        std::filesystem::path path;
        std::error_code code;
        std::string message;
    };

    /**
     * @brief Текущее состояние прогресса сканирования.
     */
    struct ScanProgress
    {
        std::filesystem::path currentPath;
        std::uint64_t processedFiles = 0;
        std::uint64_t processedDirectories = 0;
    };

    using ScanProgressCallback =
        std::function<void(const ScanProgress&)>;

    using ScanErrorCallback =
        std::function<void(const ScanError&)>;

    /**
     * @brief Интерфейс рекурсивного сканера файловой системы.
     *
     * Сканер строит дерево DirectoryNode и заполняет только direct-статистику.
     * Вычисление subtree выполняется отдельным DirectoryAggregator.
     */
    class IDirectoryScanner
    {
    public:
        virtual ~IDirectoryScanner() = default;

        /**
         * @brief Выполняет синхронный обход указанного каталога.
         *
         * @param rootPath Корневой каталог анализа.
         * @param result Объект, в который будет записан результат обхода.
         * @param cancellationRequested Флаг запроса отмены.
         * @param progressCallback Callback для уведомления о прогрессе.
         * @param errorCallback Callback для некритических ошибок.
         * @return Итоговый статус сканирования.
         *
         * Ошибки отдельных файлов и каталогов передаются через errorCallback
         * и не останавливают анализ, если продолжение обхода возможно.
         */
        virtual ScanStatus scan(
            const std::filesystem::path& rootPath,
            AnalysisResult& result,
            const std::atomic_bool& cancellationRequested,
            const ScanProgressCallback& progressCallback = {},
            const ScanErrorCallback& errorCallback = {}) const = 0;
    };
}
