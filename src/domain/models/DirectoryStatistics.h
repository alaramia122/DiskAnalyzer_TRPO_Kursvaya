#pragma once
#include <cstdint>
#include <array>
#include "../enums/FileCategory.h"


namespace domain
{
    /**
     * @brief Статистика файлов по категориям.
     *
     * Хранит суммарный размер и количество файлов
     * для каждой категории.
     */
    struct CategoryStatistics
    {
        /**
         * @brief Суммарный размер файлов по категориям.
         *
         * Индекс массива соответствует значению FileCategory.
         */
        std::array<uintmax_t, FileCategoryCount> sizesBytes{};

        /**
         * @brief Количество файлов по категориям.
         *
         * Индекс массива соответствует значению FileCategory.
         */
        std::array<uintmax_t, FileCategoryCount> filesCount{};
    };

    /**
     * @brief Статистика файлов в пределах определенной области.
     *
     * Используется как для текущей директории (direct),
     * так и для всего поддерева (subtree).
     */
    struct ScopeStatistics
    {
        /**
         * @brief Общее количество файлов.
         */
        uint64_t filesCount = 0;

        /**
         * @brief Суммарный размер файлов в байтах.
         */
        uint64_t filesSizeBytes = 0;

        /**
         * @brief Статистика по категориям файлов.
         */
        CategoryStatistics categories;
    };

    /**
     * @brief Полная статистика директории.
     *
     * Содержит статистику:
     * - только текущей директории;
     * - всей директории вместе с вложенными каталогами.
     */
    struct DirectoryStatistics
    {
        /**
         * @brief Статистика только текущей директории.
         */
        ScopeStatistics direct;

        /**
         * @brief Статистика директории и всего ее поддерева.
         */
        ScopeStatistics subtree;

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
        void mergeSubtree(const DirectoryStatistics& import_subtree);

        std::uintmax_t getSubtreeCategorySize(FileCategory cat) const;
        std::uintmax_t getSubtreeCategoryCount(FileCategory cat) const;
        std::uintmax_t getDirectCategorySize(FileCategory cat) const;
        std::uintmax_t getDirectCategoryCount(FileCategory cat) const;
        std::uintmax_t getTotalCategorySize(FileCategory cat) const;
        std::uintmax_t getTotalCategoryCount(FileCategory cat) const;
    };

} // namespace domain