#pragma once
#include <cstdint>
#include <array>
#include "../enums/FileCategory.h"

namespace domain {

    /**
     * @brief Статистика по одной директории.
     *
     * @details Содержит как прямую статистику (только содержимое директории),
     *          так и агрегированную статистику всего поддерева.
     *          Используется в DirectoryNode и AnalysisResult.
     */
    struct DirectoryStatistics {
        // Прямая статистика (только объекты непосредственно в этой директории)
        std::uintmax_t directFilesCount = 0;
        std::uintmax_t directFilesSizeBytes = 0;

        // Статистика поддерева (только вложенные директории)
        std::uintmax_t subtreeFilesCount = 0;
        std::uintmax_t subtreeSizeBytes = 0;

        /** @brief Статистика по категориям (фиксированный размер) */
        static constexpr std::size_t CategoryCount = FileCategoryCount;
        std::array<std::uintmax_t, CategoryCount> categorySizesBytes{};
        std::array<std::uintmax_t, CategoryCount> categoryFilesCount{};

        /**
         * @brief Добавляет информацию об одном файле в прямую статистику.
         * @param category Категория файла
         * @param sizeBytes Размер файла в байтах
         */
        void addFile(FileCategory category, std::uintmax_t sizeBytes);

        /**
         * @brief Сливает статистику поддерева в текущую (bottom-up агрегация).
         * @param subtree Статистика дочерней директории
         * @details Не изменяет direct-статистику.
         */
        void mergeSubtree(const DirectoryStatistics& subtree);

        std::uintmax_t getCategorySize(FileCategory cat) const;
        std::uintmax_t getCategoryCount(FileCategory cat) const;
    };

} // namespace domain