#pragma once

#include "../../domain/interfaces/IFileClassifier.h"
#include "../../domain/models/FileInfo.h"

#include <filesystem>
#include <string>
#include <vector>

namespace infrastructure
{
    /** Ошибка чтения конкретного элемента; остальные элементы могут быть доступны. */
    struct DirectoryReadError
    {
        std::filesystem::path path;
        std::string message;
    };

    /** Результат чтения одного уровня; opened отличает пустой каталог от ошибки открытия. */
    struct DirectoryReadResult
    {
        std::vector<domain::FileInfo> entries;
        std::vector<DirectoryReadError> errors;
        bool opened = false;
    };

    /**
     * Reads one directory level only. It does not recurse and does not alter
     * the analysis tree; this service is intended for GUI navigation.
     */
    class DirectoryContentsReader final
    {
    public:
        explicit DirectoryContentsReader(const domain::IFileClassifier& classifier);

        DirectoryReadResult read(const std::filesystem::path& directory) const;

    private:
        const domain::IFileClassifier& classifier_;
    };
}
