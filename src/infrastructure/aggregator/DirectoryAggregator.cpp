#include "DirectoryAggregator.h"

#include <vector>

namespace infrastructure
{
    bool DirectoryAggregator::aggregate(
        domain::AnalysisResult& result) const
    {
        domain::DirectoryNode* root = result.getRoot();

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

            for (const std::unique_ptr<domain::DirectoryNode>& child :
                 current->children)
            {
                stack.push_back(child.get());
            }
        }

        for (std::vector<domain::DirectoryNode*>::reverse_iterator iterator =
                 traversalOrder.rbegin();
             iterator != traversalOrder.rend();
             ++iterator)
        {
            domain::DirectoryNode* current = *iterator;

            // Повторный вызов aggregate() должен быть идемпотентным.
            current->stats.subtree = domain::ScopeStatistics{};

            for (const std::unique_ptr<domain::DirectoryNode>& child :
                 current->children)
            {
                current->stats.mergeSubtree(child->stats);
                if (!child->scanComplete)
                {
                    current->scanComplete = false;
                }
            }
        }

        return true;
    }
}
