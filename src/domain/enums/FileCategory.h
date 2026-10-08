#pragma once

#include <cstddef>
#include <cstdint>

namespace domain
{
    /**
     * @brief Категория файла для статистики и визуализации.
     */
    enum class FileCategory : std::uint8_t
    {
        Unknown,
        Documents,
        Images,
        Video,
        Audio,
        Archives,
        Executables,
        SourceCode,
        System,
        Other
    };

    constexpr std::size_t FileCategoryCount = 10;
}
