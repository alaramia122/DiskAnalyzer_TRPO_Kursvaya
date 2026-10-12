#pragma once

#include "../../domain/models/DirectoryStatistics.h"

#include <array>
#include <cstdint>

namespace visualization
{
    /** Область статистики, которую нужно представить на диаграмме. */
    enum class StatisticsScope
    {
        Direct,
        Subtree,
        Total
    };

    /** Данные одной категории для построения диаграммы. */
    struct CategoryValue
    {
        domain::FileCategory category = domain::FileCategory::Unknown;
        std::uint64_t filesCount = 0;
        std::uint64_t sizeBytes = 0;
    };

    /**
     * @brief Подготавливает статистику доменной модели для визуализации.
     *
     * Не зависит от Qt и не обращается к файловой системе. Получает данные
     * только из DirectoryStatistics, сформированной при анализе.
     */
    class VisualizationModel final
    {
    public:
        /**
         * @brief Возвращает статистику каждой категории для выбранной области.
         * @param statistics Статистика выбранного каталога.
         * @param scope Непосредственный каталог, вложенные каталоги или сумма.
         * @return Массив значений в порядке перечисления FileCategory.
         */
        std::array<CategoryValue, domain::FileCategoryCount> buildCategoryValues(
            const domain::DirectoryStatistics& statistics,
            StatisticsScope scope) const;
    };
}
