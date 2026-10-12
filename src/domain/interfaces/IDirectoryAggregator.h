#pragma once

#include "../models/AnalysisResult.h"

namespace domain
{
    /**
     * @brief Интерфейс агрегации статистики дерева каталогов.
     *
     * Агрегатор работает только с уже построенным деревом DirectoryNode
     * и не обращается к файловой системе.
     */
    class IDirectoryAggregator
    {
    public:
        virtual ~IDirectoryAggregator() = default;

        /**
         * @brief Вычисляет subtree-статистику для всего дерева.
         * @param result Результат сканирования с построенным деревом.
         * @return true, если дерево существует и агрегация выполнена.
         *
         * Перед расчетом subtree ранее рассчитанное значение очищается,
         * поэтому повторный вызов не удваивает статистику.
         */
        virtual bool aggregate(AnalysisResult& result) const = 0;
    };
}
