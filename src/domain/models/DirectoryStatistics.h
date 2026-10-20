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
        /** @brief Общее количество файлов в области. */
        uint64_t filesCount = 0;

        /** @brief Суммарный размер файлов области в байтах. */
        uint64_t filesSizeBytes = 0;

        /** @brief Разбивка количества и размера файлов по категориям. */
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
        /** @brief Статистика только текущей директории, без потомков. */
        ScopeStatistics direct;

        /** @brief Статистика всех вложенных каталогов без самой директории. */
        ScopeStatistics subtree;

        /**
         * @brief Добавляет информацию об одном файле в прямую статистику.
         * @param category Категория файла
         * @param sizeBytes Размер файла в байтах
         */
        void addFile(FileCategory category, std::uintmax_t sizeBytes);

        /**
         * @brief Сливает статистику дочернего узла в subtree текущего узла.
         * @param import_subtree Статистика дочерней директории.
         * @details Не изменяет direct-статистику. Вызывающая сторона должна
         *          обеспечить корректный порядок агрегации и очистить subtree
         *          перед повторным расчетом.
         */
        void mergeSubtree(const DirectoryStatistics& import_subtree);

        /**
         * @brief Возвращает суммарный размер файлов категории во вложенных каталогах.
         * @param cat Категория файлов.
         * @return Размер в байтах; 0 для значения категории вне диапазона.
         */
        std::uintmax_t getSubtreeCategorySize(FileCategory cat) const;

        /**
         * @brief Возвращает количество файлов категории во вложенных каталогах.
         * @param cat Категория файлов.
         * @return Количество файлов; 0 для значения категории вне диапазона.
         */
        std::uintmax_t getSubtreeCategoryCount(FileCategory cat) const;

        /**
         * @brief Возвращает суммарный размер файлов категории в текущей директории.
         * @param cat Категория файлов.
         * @return Размер в байтах; 0 для значения категории вне диапазона.
         */
        std::uintmax_t getDirectCategorySize(FileCategory cat) const;

        /**
         * @brief Возвращает количество файлов категории в текущей директории.
         * @param cat Категория файлов.
         * @return Количество файлов; 0 для значения категории вне диапазона.
         */
        std::uintmax_t getDirectCategoryCount(FileCategory cat) const;

        /**
         * @brief Возвращает размер файлов категории в директории и ее поддереве.
         * @param cat Категория файлов.
         * @return Суммарный размер в байтах; 0 для значения категории вне диапазона.
         */
        std::uintmax_t getTotalCategorySize(FileCategory cat) const;

        /**
         * @brief Возвращает количество файлов категории в директории и ее поддереве.
         * @param cat Категория файлов.
         * @return Суммарное количество; 0 для значения категории вне диапазона.
         */
        std::uintmax_t getTotalCategoryCount(FileCategory cat) const;
    };

} // namespace domain
