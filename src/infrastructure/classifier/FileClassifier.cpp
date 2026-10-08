#include "FileClassifier.h"

#include <cctype>

namespace infrastructure
{
    const std::unordered_map<std::string, domain::FileCategory>
        FileClassifier::extensionMap_ =
    {
        // Документы.
        {".txt", domain::FileCategory::Documents},
        {".pdf", domain::FileCategory::Documents},
        {".doc", domain::FileCategory::Documents},
        {".docx", domain::FileCategory::Documents},
        {".xls", domain::FileCategory::Documents},
        {".xlsx", domain::FileCategory::Documents},
        {".ppt", domain::FileCategory::Documents},
        {".pptx", domain::FileCategory::Documents},
        {".odt", domain::FileCategory::Documents},
        {".ods", domain::FileCategory::Documents},
        {".odp", domain::FileCategory::Documents},
        {".rtf", domain::FileCategory::Documents},

        // Изображения.
        {".jpg", domain::FileCategory::Images},
        {".jpeg", domain::FileCategory::Images},
        {".png", domain::FileCategory::Images},
        {".gif", domain::FileCategory::Images},
        {".bmp", domain::FileCategory::Images},
        {".webp", domain::FileCategory::Images},
        {".svg", domain::FileCategory::Images},
        {".ico", domain::FileCategory::Images},
        {".tif", domain::FileCategory::Images},
        {".tiff", domain::FileCategory::Images},

        // Видео.
        {".mp4", domain::FileCategory::Video},
        {".avi", domain::FileCategory::Video},
        {".mkv", domain::FileCategory::Video},
        {".mov", domain::FileCategory::Video},
        {".wmv", domain::FileCategory::Video},
        {".webm", domain::FileCategory::Video},
        {".mpeg", domain::FileCategory::Video},
        {".mpg", domain::FileCategory::Video},

        // Аудио.
        {".mp3", domain::FileCategory::Audio},
        {".wav", domain::FileCategory::Audio},
        {".flac", domain::FileCategory::Audio},
        {".ogg", domain::FileCategory::Audio},
        {".aac", domain::FileCategory::Audio},
        {".m4a", domain::FileCategory::Audio},
        {".wma", domain::FileCategory::Audio},

        // Архивы.
        {".zip", domain::FileCategory::Archives},
        {".rar", domain::FileCategory::Archives},
        {".7z", domain::FileCategory::Archives},
        {".tar", domain::FileCategory::Archives},
        {".gz", domain::FileCategory::Archives},
        {".bz2", domain::FileCategory::Archives},
        {".xz", domain::FileCategory::Archives},

        // Исполняемые файлы.
        {".exe", domain::FileCategory::Executables},
        {".com", domain::FileCategory::Executables},
        {".msi", domain::FileCategory::Executables},
        {".appimage", domain::FileCategory::Executables},
        {".run", domain::FileCategory::Executables},
        {".deb", domain::FileCategory::Executables},
        {".rpm", domain::FileCategory::Executables},

        // Исходный код.
        {".c", domain::FileCategory::SourceCode},
        {".h", domain::FileCategory::SourceCode},
        {".cpp", domain::FileCategory::SourceCode},
        {".hpp", domain::FileCategory::SourceCode},
        {".cc", domain::FileCategory::SourceCode},
        {".cxx", domain::FileCategory::SourceCode},
        {".java", domain::FileCategory::SourceCode},
        {".py", domain::FileCategory::SourceCode},
        {".js", domain::FileCategory::SourceCode},
        {".ts", domain::FileCategory::SourceCode},
        {".html", domain::FileCategory::SourceCode},
        {".css", domain::FileCategory::SourceCode},
        {".sql", domain::FileCategory::SourceCode},
        {".cs", domain::FileCategory::SourceCode},

        // Системные файлы.
        {".sys", domain::FileCategory::System},
        {".dll", domain::FileCategory::System},
        {".so", domain::FileCategory::System},
        {".dylib", domain::FileCategory::System},
        {".drv", domain::FileCategory::System}
    };

    domain::FileCategory FileClassifier::classify(
        const domain::FileInfo& file) const
    {
        std::string extension = file.path.extension().string();

        for (char& character : extension)
        {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }

        const auto iterator = extensionMap_.find(extension);

        if (iterator != extensionMap_.end())
        {
            return iterator->second;
        }

        return domain::FileCategory::Other;
    }
}
