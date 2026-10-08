#include "DirectoryScanner.h"

#include <filesystem>
#include <system_error>
#include <vector>

namespace infrastructure
{
    DirectoryScanner::DirectoryScanner(
        const domain::IFileClassifier& classifier)
        : classifier_(classifier)
    {
    }

    domain::ScanStatus DirectoryScanner::scan(
        const std::filesystem::path& rootPath,
        domain::AnalysisResult& result,
        const std::atomic_bool& cancellationRequested,
        const domain::ScanProgressCallback& progressCallback,
        const domain::ScanErrorCallback& errorCallback) const
    {
        result.setRoot(nullptr);

        std::error_code rootStatusError;
        const std::filesystem::file_status rootStatus =
            std::filesystem::status(rootPath, rootStatusError);

        if (rootStatusError)
        {
            if (errorCallback)
            {
                errorCallback({
                    rootPath,
                    rootStatusError,
                    rootStatusError.message()
                });
            }

            return domain::ScanStatus::Failed;
        }

        if (!std::filesystem::is_directory(rootStatus))
        {
            const std::error_code error =
                std::make_error_code(std::errc::not_a_directory);

            if (errorCallback)
            {
                errorCallback({
                    rootPath,
                    error,
                    "Указанный путь не является директорией."
                });
            }

            return domain::ScanStatus::Failed;
        }

        std::unique_ptr<domain::DirectoryNode> rootNode =
            std::make_unique<domain::DirectoryNode>(rootPath);

        result.setRoot(std::move(rootNode));

        std::uint64_t processedFiles = 0;
        std::uint64_t processedDirectories = 1;
        std::uint64_t processedObjectsSinceProgress = 0;

        if (progressCallback)
        {
            progressCallback({
                rootPath,
                processedFiles,
                processedDirectories
            });
        }

        std::error_code iteratorError;

        std::filesystem::recursive_directory_iterator iterator(
            rootPath,
            std::filesystem::directory_options::skip_permission_denied,
            iteratorError);

        if (iteratorError)
        {
            if (errorCallback)
            {
                errorCallback({
                    rootPath,
                    iteratorError,
                    iteratorError.message()
                });
            }

            return domain::ScanStatus::Failed;
        }

        std::vector<domain::DirectoryNode*> directoryStack;
        directoryStack.push_back(result.getRoot());

        while (iterator != std::filesystem::recursive_directory_iterator())
        {
            if (cancellationRequested.load())
            {
                return domain::ScanStatus::Cancelled;
            }

            const std::filesystem::directory_entry& entry = *iterator;
            const std::size_t depth =
                static_cast<std::size_t>(iterator.depth());

            if (depth >= directoryStack.size())
            {
                iterator.increment(iteratorError);
                continue;
            }

            directoryStack.resize(depth + 1);

            std::error_code entryTypeError;
            const std::filesystem::file_status entryStatus =
                entry.symlink_status(entryTypeError);

            if (entryTypeError)
            {
                if (errorCallback)
                {
                    errorCallback({
                        entry.path(),
                        entryTypeError,
                        entryTypeError.message()
                    });
                }

                iterator.increment(iteratorError);
                continue;
            }

            if (std::filesystem::is_directory(entryStatus))
            {
                domain::DirectoryNode* child =
                    directoryStack[depth]->addChild(entry.path());

                directoryStack.push_back(child);
                ++processedDirectories;
            }
            else if (std::filesystem::is_regular_file(entryStatus))
            {
                std::error_code fileSizeError;
                const std::uintmax_t fileSize =
                    entry.file_size(fileSizeError);

                if (fileSizeError)
                {
                    if (errorCallback)
                    {
                        errorCallback({
                            entry.path(),
                            fileSizeError,
                            fileSizeError.message()
                        });
                    }
                }
                else
                {
                    domain::FileInfo fileInfo;
                    fileInfo.path = entry.path();
                    fileInfo.sizeBytes = fileSize;
                    fileInfo.type =
                        domain::FileSystemObjectType::File;
                    fileInfo.category = classifier_.classify(fileInfo);

                    directoryStack[depth]->stats.addFile(
                        fileInfo.category,
                        fileInfo.sizeBytes);

                    ++processedFiles;
                }
            }

            ++processedObjectsSinceProgress;

            if (progressCallback &&
                processedObjectsSinceProgress >= ProgressInterval)
            {
                progressCallback({
                    entry.path(),
                    processedFiles,
                    processedDirectories
                });

                processedObjectsSinceProgress = 0;
            }

            iterator.increment(iteratorError);

            if (iteratorError)
            {
                if (errorCallback)
                {
                    errorCallback({
                        entry.path(),
                        iteratorError,
                        iteratorError.message()
                    });
                }

                iteratorError.clear();
            }
        }

        if (cancellationRequested.load())
        {
            return domain::ScanStatus::Cancelled;
        }

        if (progressCallback)
        {
            progressCallback({
                rootPath,
                processedFiles,
                processedDirectories
            });
        }

        return domain::ScanStatus::Completed;
    }
}
