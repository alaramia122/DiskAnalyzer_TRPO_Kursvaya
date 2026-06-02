#pragma once

namespace domain
{
    /**
     * @brief Тип объекта файловой системы.
     *
     * @details Разделяет директории и файлы.
     *          Directory не является категорией файла (FileCategory).
     */
    enum class FileSystemObjectType : std::uint8_t
    {
        File,       ///< Обычный файл
        Directory   ///< Директория
    };

}   // namespace domain