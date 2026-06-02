#pragma once

namespace domain
{
    /**
     * @brief Состояние процесса сканирования файловой системы.
     *
     * @details Используется для отображения прогресса и состояния в GUI.
     */
    enum class ScanStatus : std::uint8_t
    {
        Idle,       ///< Анализ не запущен
        Running,    ///< Анализ в процессе
        Completed,  ///< Анализ успешно завершен
        Cancelled,  ///< Анализ отменен пользователем
        Failed      ///< Ошибка при анализе
    };

}   // namespace domain