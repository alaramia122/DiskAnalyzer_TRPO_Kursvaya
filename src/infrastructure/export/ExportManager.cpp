#include "ExportManager.h"

#include "../../domain/enums/FileCategory.h"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>

namespace
{
    const char* categoryName(domain::FileCategory category)
    {
        switch (category)
        {
        case domain::FileCategory::Unknown: return "Unknown";
        case domain::FileCategory::Documents: return "Documents";
        case domain::FileCategory::Images: return "Images";
        case domain::FileCategory::Video: return "Video";
        case domain::FileCategory::Audio: return "Audio";
        case domain::FileCategory::Archives: return "Archives";
        case domain::FileCategory::Executables: return "Executables";
        case domain::FileCategory::SourceCode: return "SourceCode";
        case domain::FileCategory::System: return "System";
        case domain::FileCategory::Other: return "Other";
        }
        return "Unknown";
    }

    std::string pathToUtf8(const std::filesystem::path& path)
    {
        return path.u8string();
    }

    void writeJsonString(std::ostream& output, const std::string& value)
    {
        output << '"';
        for (const unsigned char character : value)
        {
            switch (character)
            {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    const char hex[] = "0123456789abcdef";
                    output << "\\u00"
                           << hex[(character >> 4) & 0x0f]
                           << hex[character & 0x0f];
                }
                else
                {
                    output << static_cast<char>(character);
                }
                break;
            }
        }
        output << '"';
    }

    void writeScopeJson(
        std::ostream& output,
        const domain::ScopeStatistics& statistics)
    {
        output << "{\n  \"filesCount\": " << statistics.filesCount
               << ",\n  \"filesSizeBytes\": " << statistics.filesSizeBytes
               << ",\n  \"categories\": [";
        for (std::size_t index = 0; index < domain::FileCategoryCount; ++index)
        {
            output << (index == 0 ? "\n" : ",\n")
                   << "    {\"name\": \"" << categoryName(
                       static_cast<domain::FileCategory>(index))
                   << "\", \"filesCount\": "
                   << statistics.categories.filesCount[index]
                   << ", \"sizeBytes\": "
                   << statistics.categories.sizesBytes[index] << "}";
        }
        output << "\n  ]\n}";
    }

    void writeDirectoryJson(
        std::ostream& output,
        const domain::DirectoryNode& node,
        int indentation)
    {
        const std::string indent(static_cast<std::size_t>(indentation), ' ');
        output << indent << "{\n" << indent << "  \"path\": ";
        writeJsonString(output, pathToUtf8(node.path));
        output << ",\n" << indent << "  \"direct\": ";
        writeScopeJson(output, node.stats.direct);
        output << ",\n" << indent << "  \"subtree\": ";
        writeScopeJson(output, node.stats.subtree);
        output << ",\n" << indent << "  \"children\": [";
        for (std::size_t index = 0; index < node.children.size(); ++index)
        {
            output << (index == 0 ? "\n" : ",\n");
            writeDirectoryJson(output, *node.children[index], indentation + 4);
        }
        if (!node.children.empty())
        {
            output << "\n" << indent << "  ";
        }
        output << "]\n" << indent << "}";
    }

    void writeScopeText(
        std::ostream& output,
        const char* title,
        const domain::ScopeStatistics& statistics)
    {
        output << "  " << title << ": файлов=" << statistics.filesCount
               << ", размер=" << statistics.filesSizeBytes << " байт\n";
        for (std::size_t index = 0; index < domain::FileCategoryCount; ++index)
        {
            output << "    " << categoryName(
                static_cast<domain::FileCategory>(index))
                   << ": " << statistics.categories.filesCount[index]
                   << " файлов, "
                   << statistics.categories.sizesBytes[index] << " байт\n";
        }
    }

    void writeDirectoryText(
        std::ostream& output,
        const domain::DirectoryNode& node,
        std::size_t depth)
    {
        output << std::string(depth * 2, ' ')
               << "Каталог: " << pathToUtf8(node.path) << '\n';
        writeScopeText(output, "Непосредственно", node.stats.direct);
        writeScopeText(output, "Вложенные каталоги", node.stats.subtree);
        for (const auto& child : node.children)
        {
            writeDirectoryText(output, *child, depth + 1);
        }
    }
}

namespace infrastructure
{
    bool ExportManager::exportResult(
        const domain::AnalysisResult& result,
        const std::filesystem::path& outputPath,
        domain::ExportFormat format,
        std::string* errorMessage) const
    {
        if (errorMessage)
        {
            errorMessage->clear();
        }

        if (result.getRoot() == nullptr)
        {
            if (errorMessage)
            {
                *errorMessage = "Нет результатов анализа для экспорта.";
            }
            return false;
        }

        if (format != domain::ExportFormat::Json &&
            format != domain::ExportFormat::Txt)
        {
            if (errorMessage)
            {
                *errorMessage = "Неизвестный формат экспорта.";
            }
            return false;
        }

        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            if (errorMessage)
            {
                *errorMessage = "Не удалось открыть файл для записи.";
            }
            return false;
        }

        if (format == domain::ExportFormat::Json)
        {
            output << "{\n  \"root\": ";
            writeDirectoryJson(output, *result.getRoot(), 4);
            output << "\n}\n";
        }
        else if (format == domain::ExportFormat::Txt)
        {
            output << "Результаты анализа файловой системы\n";
            writeDirectoryText(output, *result.getRoot(), 0);
        }
        output.flush();
        if (!output)
        {
            if (errorMessage)
            {
                *errorMessage = "Ошибка при записи файла.";
            }
            return false;
        }

        return true;
    }
}
