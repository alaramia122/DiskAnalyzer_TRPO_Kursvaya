#pragma once

namespace domain
{
    /**
     * @brief Перечисление категорий файлов для классификации и статистики.
     *
     * @details Используется для агрегации статистики по типам файлов
     *          и построения круговых диаграмм в GUI.
     *          Набор категорий фиксирован для эффективности (std::array).
     */
    enum class FileCategory : std::uint8_t
    {
		Unknown,      ///< Неизвестная категория. Устанавливается как значение по умолчанию при создании FileInfo.
        Documents,    ///< Документы (pdf, doc, xlsx и т.д.)
        Images,       ///< Изображения
        Video,        ///< Видео файлы
        Audio,        ///< Аудио файлы
        Archives,     ///< Архивы (zip, rar, 7z)
        Executables,  ///< Исполняемые файлы
        SourceCode,   ///< Исходный код
        System,       ///< Системные файлы
        Other         ///< Прочие файлы
    };

    constexpr std::size_t FileCategoryCount = 10;
}       // namespace domain