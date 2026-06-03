#include "DirectoryNode.h"

#include <utility>

namespace domain
{

    // Конструктор узла дерева каталогов.
    // Создает узел для указанного пути.
    // Родитель по умолчанию отсутствует.
    DirectoryNode::DirectoryNode(std::filesystem::path p)
        : path(std::move(p))
    {
    }

    // Создает дочерний узел, автоматически связывает его
    // с текущим узлом и возвращает указатель на созданный объект.
    //
    // Владение памятью полностью остается внутри дерева:
    // дочерний узел хранится в контейнере children.
    DirectoryNode* DirectoryNode::addChild(
        const std::filesystem::path& path)
    {
        auto child = std::make_unique<DirectoryNode>(path);

        child->parent = this;

        DirectoryNode* result = child.get();

        children.push_back(std::move(child));

        return result;
    }

    // Ищет дочерний каталог среди непосредственных потомков
    // текущего узла по имени каталога.
    //
    // Возвращает:
    // - указатель на найденный дочерний узел;
    // - nullptr, если дочерний каталог не найден.
    DirectoryNode* DirectoryNode::findChild(
        const std::string& childName) const
    {
        for (const auto& child : children)
        {
            if (child->path.filename().string() == childName)
            {
                return child.get();
            }
        }

        return nullptr;
    }
}