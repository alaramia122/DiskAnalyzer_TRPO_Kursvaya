#pragma once
#include "DirectoryStatistics.h"
#include <filesystem>
#include <vector>
#include <memory>
#include <string>

namespace domain {

    /**
     * @brief Узел дерева каталогов.
     *
     * @details Представляет одну директорию в иерархии.
     *          Хранит статистику и связи для навигации.
     *          Полное дерево файлов не хранится — только каталоги.
     */
    class DirectoryNode {
    public:
        std::filesystem::path path;           ///< Полный путь к директории
        DirectoryStatistics stats;            ///< Статистика директории
        bool scanComplete = true;             ///< Все элементы каталога удалось проверить

        DirectoryNode* parent = nullptr;      ///< Указатель на родителя
        std::vector<std::unique_ptr<DirectoryNode>> children; ///< Дочерние директории

        /**
         * @brief Конструктор узла.
         * @param p Путь к директории
         */
        explicit DirectoryNode(std::filesystem::path p);

        /**
         * @brief Создает дочерний каталог и добавляет его в дерево.
         *
         * Метод автоматически устанавливает связь родитель-потомок
         * и возвращает указатель на созданный узел.
         *
         * @param path Путь к создаваемому каталогу.
         * @return Указатель на созданный дочерний узел.
         */
        DirectoryNode* addChild(const std::filesystem::path& path);

        /**
         * @brief Находит дочерний узел по имени.
         * @param childName Имя дочерней директории
         * @return Указатель на найденный узел или nullptr
         */
        DirectoryNode* findChild(const std::string& childName) const;
    };

} // namespace domain