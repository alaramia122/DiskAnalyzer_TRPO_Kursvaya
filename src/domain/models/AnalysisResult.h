#pragma once

#include "DirectoryNode.h"
#include <filesystem>
#include <memory>

namespace domain {

    /**
     * @brief Корневой результат анализа файловой системы.
     *
     * Владеет полным деревом каталогов.
     * Поиск осуществляется обходом дерева от root.
     * Индекс по пути убран как избыточный для локальной навигации.
     */
    class AnalysisResult {
    public:
        AnalysisResult() = default;

        /**
         * @brief Устанавливает корневой узел (передаёт владение).
         */
        void setRoot(std::unique_ptr<DirectoryNode> rootNode);

        /**
         * @brief Возвращает корневой узел.
         */
        DirectoryNode* getRoot() const;

        /**
         * @brief Находит узел по пути (обходом дерева от root).
         * @param path Полный путь к директории.
         * @return Указатель на узел или nullptr.
         *
         * Примечание: Пока используется редко, т.к. навигация локальная.
         */
        DirectoryNode* findNode(const std::filesystem::path& path) const;

        /**
         * @brief Получает статистику по пути.
         * Возвращает копию (immutable snapshot).
         */
        DirectoryStatistics getStatistics(const std::filesystem::path& path) const;

        /**
         * @brief Проверяет, содержит ли результат данные.
         */
        bool isEmpty() const;

        // === Локальная навигация ===

        /**
         * @brief Устанавливает текущий узел навигации.
         */
        void setCurrentNode(DirectoryNode* node);

        /**
         * @brief Возвращает текущий узел навигации.
         */
        DirectoryNode* getCurrentNode() const;

        /**
         * @brief Переходит к родительской директории.
         * @return true, если переход успешен.
         */
        bool navigateToParent();

        /**
         * @brief Переходит к дочерней директории по имени.
         * @return true, если ребёнок найден.
         */
        bool navigateToChild(const std::string& childName);

        /**
         * @brief Возвращает статистику текущей директории.
         */
        const DirectoryStatistics& getCurrentStatistics() const;

    private:
        /** Корневой узел дерева */
        std::unique_ptr<DirectoryNode> root_;

        /**
         * @brief Текущий отображаемый узел дерева каталогов.
         *
         * Используется для навигации между каталогами и получения
         * статистики текущего уровня без необходимости повторного
         * поиска по дереву.
         */
        DirectoryNode* currentNode_ = nullptr;

        /**
         * @brief Рекурсивный поиск узла по пути.
         */
        DirectoryNode* findNodeRecursive(DirectoryNode* current,
            const std::filesystem::path& targetPath) const;
    };

} // namespace domain