#include "DirectoryStatistics.h"


namespace domain {

    // addFile изменяет только direct-область: статистика вложенных каталогов
    // рассчитывается отдельно агрегатором после завершения сканирования.
    void DirectoryStatistics::addFile(FileCategory category, std::uintmax_t sizeBytes)
    {
        // 1. Обновляем ПРЯМУЮ статистику текущей директории
        direct.filesCount++;                    // +1 файл непосредственно в папке
        direct.filesSizeBytes += sizeBytes;     // добавляем размер файла

        // 2. Обновляем статистику по категориям
        std::size_t idx = static_cast<std::size_t>(category);

        // Защита от некорректного значения enum (defense programming)
        if (idx < FileCategoryCount)
        {
            direct.categories.filesCount[idx]++;         // увеличиваем счётчик файлов категории
            direct.categories.sizesBytes[idx] += sizeBytes; // увеличиваем суммарный размер категории
        }
        else
        {
            // Если вдруг категория вышла за пределы (ошибка в коде) — падаем в Unknown
            direct.categories.filesCount[static_cast<std::size_t>(FileCategory::Unknown)]++;
            direct.categories.sizesBytes[static_cast<std::size_t>(FileCategory::Unknown)] += sizeBytes;
        }
    }

    // Суммирует статистику дочернего поддерева в subtree текущего узла.
    // Перед повторной агрегацией вызывающая сторона должна очистить subtree.
    void DirectoryStatistics::mergeSubtree(const DirectoryStatistics& import_subtree)
    {
        // Добавляем статистику из поддерева (только вложенные папки)
        subtree.filesCount += import_subtree.direct.filesCount + import_subtree.subtree.filesCount;
        subtree.filesSizeBytes += import_subtree.direct.filesSizeBytes + import_subtree.subtree.filesSizeBytes;

        // Категории тоже суммируем полностью
        for (std::size_t i = 0; i < FileCategoryCount; ++i)
        {
            subtree.categories.filesCount[i] += import_subtree.subtree.categories.filesCount[i] + import_subtree.direct.categories.filesCount[i];
            subtree.categories.sizesBytes[i] += import_subtree.subtree.categories.sizesBytes[i] + import_subtree.direct.categories.sizesBytes[i];
        }
    }

    std::uintmax_t DirectoryStatistics::getSubtreeCategorySize(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < FileCategoryCount) ? subtree.categories.sizesBytes[idx] : 0;
    }

    std::uintmax_t DirectoryStatistics::getSubtreeCategoryCount(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < FileCategoryCount) ? subtree.categories.filesCount[idx] : 0;
    }

    std::uintmax_t DirectoryStatistics::getDirectCategorySize(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < FileCategoryCount) ? direct.categories.sizesBytes[idx] : 0;
    }

    std::uintmax_t DirectoryStatistics::getDirectCategoryCount(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < FileCategoryCount) ? direct.categories.filesCount[idx] : 0;
    }

    std::uintmax_t DirectoryStatistics::getTotalCategorySize(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < FileCategoryCount) ? (direct.categories.sizesBytes[idx] + subtree.categories.sizesBytes[idx]) : 0;
    }

    std::uintmax_t DirectoryStatistics::getTotalCategoryCount(FileCategory cat) const
    {
        std::size_t idx = static_cast<std::size_t>(cat);
        return (idx < FileCategoryCount) ? (direct.categories.filesCount[idx] + subtree.categories.filesCount[idx]) : 0;
    }

} // namespace domain