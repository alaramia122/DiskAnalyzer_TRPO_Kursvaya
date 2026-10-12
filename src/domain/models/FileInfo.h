#pragma once
#include <filesystem>
#include <cstdint>
#include "../enums/FileCategory.h"
#include "../enums/FileSystemObjectType.h"

namespace domain {

    /**
     * @brief Информация об одном объекте файловой системы.
     *
     * @details Временная структура, используемая в конвейере:
     *          Scanner → Classifier → Aggregator.
     *          После агрегации объекты FileInfo уничтожаются.
     */
    struct FileInfo {
        std::filesystem::path path;                    ///< Полный путь
        std::uintmax_t sizeBytes = 0;                  ///< Размер в байтах
        FileCategory category = FileCategory::Unknown; ///< Категория файла
        FileSystemObjectType type = FileSystemObjectType::File; ///< Тип объекта
    };

} // namespace domain