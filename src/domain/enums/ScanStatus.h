#pragma once

#include <cstdint>

namespace domain
{
    /**
     * @brief Состояние процесса сканирования файловой системы.
     */
    enum class ScanStatus : std::uint8_t
    {
        Idle,
        Running,
        Completed,
        Cancelled,
        Failed
    };
}
