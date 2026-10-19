#include "DirectoryAggregator.h"

#include <vector>

namespace infrastructure
{
    // Агрегирует статистику потомков в каждом узле дерева.
    bool DirectoryAggregator::aggregate(
        domain::AnalysisResult& result) const
    {
        domain::DirectoryNode* root = result.getRoot();

        // Без корневого узла обходить и агрегировать нечего.
        if (!root)
        {
            return false;
        }

        // Сначала формируем порядок обхода от корня к листьям.
        // Затем обрабатываем его в обратном порядке, что дает
        // bottom-up агрегацию без рекурсии и риска переполнения стека
        // на глубоком дереве каталогов.
        std::vector<domain::DirectoryNode*> traversalOrder;
        std::vector<domain::DirectoryNode*> stack;

        stack.push_back(root);

        while (!stack.empty())
        {
            domain::DirectoryNode* current = stack.back();
            stack.pop_back();

            traversalOrder.push_back(current);

            // Добавляем дочерние узлы в стек для последующего обхода.
            for (const std::unique_ptr<domain::DirectoryNode>& child :
                 current->children)
            {
                stack.push_back(child.get());
            }
        }

        // Обратный порядок гарантирует, что статистика каждого ребёнка
        // уже агрегирована к моменту обработки его родителя.
        for (std::vector<domain::DirectoryNode*>::reverse_iterator iterator =
                 traversalOrder.rbegin();
             iterator != traversalOrder.rend();
             ++iterator)
        {
            domain::DirectoryNode* current = *iterator;

            // Сбрасываем только subtree-статистику: direct-статистика
            // была собрана сканером и используется как исходное значение.
            // Это также делает повторный вызов aggregate() идемпотентным.
            current->stats.subtree = domain::ScopeStatistics{};

            for (const std::unique_ptr<domain::DirectoryNode>& child :
                 current->children)
            {
                // Суммируем статистику дочернего поддерева в текущем узле.
                current->stats.mergeSubtree(child->stats);

                // Если часть дочернего каталога прочитать не удалось,
                // родительское поддерево тоже нельзя считать полным.
                if (!child->scanComplete)
                {
                    current->scanComplete = false;
                }
            }
        }

        return true;
    }
}
