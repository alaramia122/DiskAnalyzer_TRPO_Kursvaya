#pragma once

#include <cstdint>

namespace domain
{
    /**
     * @brief Тип объекта файловой системы.
     *
     * Используется для различения обычных файлов и каталогов в результатах
     * чтения содержимого. Символическая ссылка определяется отдельно средствами
     * filesystem и не представлена самостоятельным значением этого enum.
     */
    enum class FileSystemObjectType : std::uint8_t
    {
        File,      ///< Обычный файл
        Directory  ///< Каталог
    };
}
