#pragma once

#include "../../domain/models/FileInfo.h"

#include <cstddef>
#include <filesystem>
#include <list>
#include <unordered_map>
#include <vector>

namespace infrastructure
{
    /**
     * @brief Ограниченный кэш сведений о файлах для GUI.
     *
     * Не выполняет ввод-вывод и не отвечает за навигацию по дереву каталогов.
     * Наполнение кэша выполняет вызывающая подсистема после локального чтения
     * содержимого каталога. Общий AnalysisResult не хранит FileInfo.
     *
     * Кэш ограничивает число каталогов, а не число файлов в каталоге:
     * текущий каталог нельзя молча обрезать, иначе GUI покажет неполный список.
     */
    class DirectoryCacheManager final
    {
    public:
        using FileList = std::vector<domain::FileInfo>;

        explicit DirectoryCacheManager(std::size_t maxCachedDirectories = 8);

        /**
         * @brief Заменяет закэшированный список файлов каталога.
         * @return false, если лимит кэша равен нулю.
         */
        bool store(const std::filesystem::path& directory, FileList files);

        /**
         * @brief Возвращает список файлов, если каталог есть в кэше.
         * Обращение обновляет позицию каталога в LRU.
         */
        const FileList* find(const std::filesystem::path& directory);

        /** Устанавливает текущий каталог и сохраняет его в кэше при наличии. */
        void setCurrentDirectory(const std::filesystem::path& directory);

        /** Возвращает текущий путь или пустой путь, если он не задан. */
        const std::filesystem::path& currentDirectory() const noexcept;

        /** Число каталогов в кэше. */
        std::size_t cachedDirectoryCount() const noexcept;

        /** Число FileInfo во всех закэшированных списках. */
        std::size_t cachedFileCount() const noexcept;

        /** Очищает кэш и сбрасывает текущий каталог. */
        void clear() noexcept;

    private:
        struct CacheEntry
        {
            FileList files;
            std::list<std::filesystem::path>::iterator lruPosition;
        };

        void touch(
            typename std::unordered_map<std::filesystem::path, CacheEntry>::iterator entry);
        void evictLeastRecentlyUsed();

        std::size_t maxCachedDirectories_;
        std::filesystem::path currentDirectory_;
        std::list<std::filesystem::path> lru_;
        std::unordered_map<std::filesystem::path, CacheEntry> entries_;
        std::size_t cachedFileCount_ = 0;
    };
}
