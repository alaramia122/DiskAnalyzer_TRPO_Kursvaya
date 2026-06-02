#include "DirectoryStatistics.h"
#include <algorithm> // для std::min

namespace domain {

    void DirectoryStatistics::addFile(FileCategory category, std::uintmax_t sizeBytes)
    {
        // Прямая статистика
        directFilesCount++;
        directFilesSizeBytes += sizeBytes;

        // Статистика по категориям
        std::size_t idx = static_cast<std::size_t>(category);
        if (idx < CategoryCount)
        {
            categoryFilesCount[idx]++;
            categorySizesBytes[idx] += sizeBytes;
        }
        else
        {
            // Защита от некорректного enum
            categoryFilesCount[static_cast<std::size_t>(FileCategory::Unknown)]++;
            categorySizesBytes[static_cast<std::size_t>(FileCategory::Unknown)] += sizeBytes;
        }
    }

    void DirectoryStatistics::mergeSubtree(const DirectoryStatistics& subtree)
    {
        // Слияние статистики поддерева
        totalFilesCount += subtree.totalFilesCount;
        totalSizeBytes += subtree.totalSizeBytes;

        for (std::size_t i = 0; i < CategoryCount; ++i)
        {
            categoryFilesCount[i] += subtree.categoryFilesCount[i];
            categorySizesBytes[i] += subtree.categorySizesBytes[i];
        }
    }

    std::uintmax_t DirectoryStatistics::getCategorySize(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < CategoryCount) ? categorySizesBytes[idx] : 0;
    }

    std::uintmax_t DirectoryStatistics::getCategoryCount(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < CategoryCount) ? categoryFilesCount[idx] : 0;
    }

} // namespace domain