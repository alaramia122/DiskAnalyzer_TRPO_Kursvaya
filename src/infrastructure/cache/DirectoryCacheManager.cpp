#include "DirectoryCacheManager.h"

#include <utility>

namespace infrastructure
{
    DirectoryCacheManager::DirectoryCacheManager(std::size_t maxCachedDirectories)
        : maxCachedDirectories_(maxCachedDirectories)
    {
    }

    bool DirectoryCacheManager::store(
        const std::filesystem::path& directory,
        FileList files)
    {
        if (maxCachedDirectories_ == 0)
        {
            return false;
        }

        const auto existing = entries_.find(directory);
        if (existing != entries_.end())
        {
            cachedFileCount_ -= existing->second.files.size();
            existing->second.files = std::move(files);
            cachedFileCount_ += existing->second.files.size();
            touch(existing);
            return true;
        }

        lru_.push_front(directory);
        CacheEntry entry{std::move(files), lru_.begin()};
        cachedFileCount_ += entry.files.size();
        entries_.emplace(directory, std::move(entry));

        while (entries_.size() > maxCachedDirectories_)
        {
            evictLeastRecentlyUsed();
        }
        return true;
    }

    const DirectoryCacheManager::FileList* DirectoryCacheManager::find(
        const std::filesystem::path& directory)
    {
        const auto found = entries_.find(directory);
        if (found == entries_.end())
        {
            return nullptr;
        }

        touch(found);
        return &found->second.files;
    }

    bool DirectoryCacheManager::invalidate(
        const std::filesystem::path& directory) noexcept
    {
        const auto found = entries_.find(directory);
        if (found == entries_.end())
        {
            return false;
        }

        cachedFileCount_ -= found->second.files.size();
        lru_.erase(found->second.lruPosition);
        entries_.erase(found);
        return true;
    }

    void DirectoryCacheManager::setCurrentDirectory(
        const std::filesystem::path& directory)
    {
        currentDirectory_ = directory;
        const auto found = entries_.find(directory);
        if (found != entries_.end())
        {
            touch(found);
        }
    }

    const std::filesystem::path&
    DirectoryCacheManager::currentDirectory() const noexcept
    {
        return currentDirectory_;
    }

    std::size_t DirectoryCacheManager::cachedDirectoryCount() const noexcept
    {
        return entries_.size();
    }

    std::size_t DirectoryCacheManager::cachedFileCount() const noexcept
    {
        return cachedFileCount_;
    }

    void DirectoryCacheManager::clear() noexcept
    {
        entries_.clear();
        lru_.clear();
        currentDirectory_.clear();
        cachedFileCount_ = 0;
    }

    void DirectoryCacheManager::touch(
        typename std::unordered_map<std::filesystem::path, CacheEntry>::iterator entry)
    {
        lru_.erase(entry->second.lruPosition);
        lru_.push_front(entry->first);
        entry->second.lruPosition = lru_.begin();
    }

    void DirectoryCacheManager::evictLeastRecentlyUsed()
    {
        if (lru_.empty())
        {
            return;
        }

        const std::filesystem::path victim = lru_.back();
        const auto found = entries_.find(victim);
        if (found != entries_.end())
        {
            cachedFileCount_ -= found->second.files.size();
            entries_.erase(found);
        }
        lru_.pop_back();
    }
}
