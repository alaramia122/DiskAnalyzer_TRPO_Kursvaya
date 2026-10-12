#pragma once

#include <cstdint>

namespace domain
{
    /**
     * @brief Тип объекта файловой системы.
     */
    enum class FileSystemObjectType : std::uint8_t
    {
        File,
        Directory
    };
}
