#pragma once

#include <cstdint>

namespace domain {

    /**
     * @brief Форматы экспорта результатов анализа.
     *
     * @details Поддерживаемые форматы для ExportManager.
     */
    enum class ExportFormat : std::uint8_t {
        Json,   ///< Экспорт в JSON (машиночитаемый)
        Txt     ///< Экспорт в TXT (человекочитаемый)
    };

} // namespace domain