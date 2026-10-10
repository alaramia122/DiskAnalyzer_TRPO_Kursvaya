#include "DirectoryContentsReader.h"

#include <system_error>
#include <utility>

namespace infrastructure
{
    DirectoryContentsReader::DirectoryContentsReader(
        const domain::IFileClassifier& classifier)
        : classifier_(classifier)
    {
    }

    DirectoryReadResult DirectoryContentsReader::read(
        const std::filesystem::path& directory) const
    {
        DirectoryReadResult result;
        std::error_code error;
        std::filesystem::directory_iterator iterator(
            directory, std::filesystem::directory_options::skip_permission_denied,
            error);
        const std::filesystem::directory_iterator end;

        if (error)
        {
            result.errors.push_back({directory, error.message()});
            return result;
        }

        result.opened = true;
        while (iterator != end)
        {
            const std::filesystem::directory_entry entry = *iterator;
            domain::FileInfo info;
            info.path = entry.path();

            std::error_code statusError;
            const auto status = entry.symlink_status(statusError);
            if (statusError)
            {
                result.errors.push_back({info.path, statusError.message()});
            }
            else if (std::filesystem::is_directory(status))
            {
                info.type = domain::FileSystemObjectType::Directory;
                info.category = domain::FileCategory::Other;
                result.entries.push_back(std::move(info));
            }
            else if (std::filesystem::is_regular_file(status))
            {
                info.type = domain::FileSystemObjectType::File;
                info.category = classifier_.classify(info);
                info.sizeBytes = entry.file_size(statusError);
                if (statusError)
                {
                    info.sizeBytes = 0;
                    result.errors.push_back({info.path, statusError.message()});
                }
                result.entries.push_back(std::move(info));
            }
            else
            {
                // Symlinks and special objects are displayed but not followed.
                info.type = domain::FileSystemObjectType::File;
                info.category = domain::FileCategory::Other;
                result.entries.push_back(std::move(info));
            }

            iterator.increment(error);
            if (error)
            {
                result.errors.push_back({directory, error.message()});
                break;
            }
        }

        return result;
    }
}
