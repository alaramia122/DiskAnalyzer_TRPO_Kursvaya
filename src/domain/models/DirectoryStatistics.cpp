#include "DirectoryStatistics.h"
#include <algorithm> // не используется в текущей версии, можно убрать позже

namespace domain {

    void DirectoryStatistics::addFile(FileCategory category, std::uintmax_t sizeBytes)
    {
        // 1. Обновляем ПРЯМУЮ статистику текущей директории
        directFilesCount++;                    // +1 файл непосредственно в папке
        directFilesSizeBytes += sizeBytes;     // добавляем размер файла

        // 2. Обновляем статистику по категориям
        std::size_t idx = static_cast<std::size_t>(category);

        // Защита от некорректного значения enum (defense programming)
        if (idx < CategoryCount)
        {
            categoryFilesCount[idx]++;         // увеличиваем счётчик файлов категории
            categorySizesBytes[idx] += sizeBytes; // увеличиваем суммарный размер категории
        }
        else
        {
            // Если вдруг категория вышла за пределы (ошибка в коде) — падаем в Unknown
            categoryFilesCount[static_cast<std::size_t>(FileCategory::Unknown)]++;
            categorySizesBytes[static_cast<std::size_t>(FileCategory::Unknown)] += sizeBytes;
        }
    }

    void DirectoryStatistics::mergeSubtree(const DirectoryStatistics& subtree)
    {
        // Добавляем статистику из поддерева (только вложенные папки)
        subtreeFilesCount += subtree.directFilesCount + subtree.subtreeFilesCount;
        subtreeSizeBytes += subtree.directFilesSizeBytes + subtree.subtreeSizeBytes;

        // Категории тоже суммируем полностью
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