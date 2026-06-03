#include "AnalysisResult.h"

#include <stdexcept>
#include <utility>

namespace domain
{

    // Устанавливает корневой узел дерева каталогов.
    // После установки корень становится текущим узлом.
    void AnalysisResult::setRoot(
        std::unique_ptr<DirectoryNode> root)
    {
        root_ = std::move(root);
        currentNode_ = root_.get();
    }

    // Возвращает указатель на корневой узел дерева.
    DirectoryNode* AnalysisResult::getRoot() const
    {
        return root_.get();
    }

    // Устанавливает текущий отображаемый узел.
    void AnalysisResult::setCurrentNode(
        DirectoryNode* node)
    {
        currentNode_ = node;
    }

    // Возвращает текущий отображаемый узел.
    DirectoryNode* AnalysisResult::getCurrentNode() const
    {
        return currentNode_;
    }

    // Переходит к родительскому каталогу.
    // Возвращает true при успешном переходе.
    bool AnalysisResult::navigateToParent()
    {
        if (!currentNode_ || !currentNode_->parent)
        {
            return false;
        }

        currentNode_ = currentNode_->parent;

        return true;
    }

    // Переходит к дочернему каталогу по имени.
    // Возвращает true при успешном переходе.
    bool AnalysisResult::navigateToChild(
        const std::string& childName)
    {
        if (!currentNode_)
        {
            return false;
        }

        DirectoryNode* child =
            currentNode_->findChild(childName);

        if (!child)
        {
            return false;
        }

        currentNode_ = child;

        return true;
    }

    // Выполняет поиск узла дерева по абсолютному пути.
    DirectoryNode* AnalysisResult::findNode(
        const std::filesystem::path& path) const
    {
        return findNodeRecursive(
            root_.get(),
            path.lexically_normal());
    }

    // Возвращает статистику текущего каталога.
    const DirectoryStatistics&
        AnalysisResult::getCurrentStatistics() const
    {

        if (!currentNode_)
        {
            throw std::runtime_error(
                "Current node is not set.");
        }

        return currentNode_->stats;
    }

    // Рекурсивно выполняет поиск узла дерева по пути.
    DirectoryNode* AnalysisResult::findNodeRecursive(
        DirectoryNode* node,
        const std::filesystem::path& path) const
    {
        if (!node)
        {
            return nullptr;
        }

        if (node->path.lexically_normal() == path)
        {
            return node;
        }

        for (const auto& child : node->children)
        {
            DirectoryNode* result =
                findNodeRecursive(child.get(), path);

            if (result)
            {
                return result;
            }
        }

        return nullptr;
    }
}