#pragma once

#include "../../domain/interfaces/IDirectoryAggregator.h"

namespace infrastructure
{
    /**
     * @brief Bottom-up агрегатор статистики дерева каталогов.
     *
     * Для каждого каталога вычисляет статистику вложенного поддерева.
     * Файловая система не используется.
     */
    class DirectoryAggregator final : public domain::IDirectoryAggregator
    {
    public:
        /**
         * @brief Выполняет агрегацию всего дерева.
         * @param result Результат сканирования.
         * @return true, если дерево существует; иначе false.
         */
        bool aggregate(domain::AnalysisResult& result) const override;
    };
}
