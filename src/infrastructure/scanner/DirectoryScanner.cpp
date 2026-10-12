#include "DirectoryScanner.h"

#include <filesystem>
#include <memory>
#include <system_error>
#include <vector>

namespace infrastructure
{
    namespace
    {
        struct DirectoryFrame
        {
            std::filesystem::directory_iterator iterator;
            domain::DirectoryNode* node = nullptr;
        };
    }

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

        std::error_code rootIteratorError;
        std::filesystem::directory_iterator rootIterator(
            rootPath,
            std::filesystem::directory_options::none,
            rootIteratorError);

        if (rootIteratorError)
        {
            result.getRoot()->scanComplete = false;
            if (errorCallback)
            {
                errorCallback({
                    rootPath,
                    rootIteratorError,
                    rootIteratorError.message()
                });
            }

            return domain::ScanStatus::Failed;
        }

        std::vector<DirectoryFrame> stack;
        stack.push_back({
            std::move(rootIterator),
            result.getRoot()
        });

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

        while (!stack.empty())
        {
            if (cancellationRequested.load())
            {
                if (result.getRoot())
                {
                    result.getRoot()->scanComplete = false;
                }
                // Помечаем также текущую ветвь: результат до отмены частичный.
                stack.back().node->scanComplete = false;
                return domain::ScanStatus::Cancelled;
            }

            DirectoryFrame& frame = stack.back();

            if (frame.iterator == std::filesystem::directory_iterator())
            {
                stack.pop_back();
                continue;
            }

            const std::filesystem::directory_entry entry = *frame.iterator;

            std::error_code incrementError;
            frame.iterator.increment(incrementError);

            if (incrementError)
            {
                frame.node->scanComplete = false;
                if (errorCallback)
                {
                    errorCallback({
                        frame.node->path,
                        incrementError,
                        incrementError.message()
                    });
                }

                // Ошибка итератора делает дальнейший обход текущего
                // каталога ненадежным. Закрываем только текущий frame;
                // остальные уровни дерева продолжают обрабатываться.
                frame.iterator =
                    std::filesystem::directory_iterator();
            }

            std::error_code linkStatusError;
            const std::filesystem::file_status linkStatus =
                entry.symlink_status(linkStatusError);

            if (linkStatusError)
            {
                frame.node->scanComplete = false;
                if (errorCallback)
                {
                    errorCallback({
                        entry.path(),
                        linkStatusError,
                        linkStatusError.message()
                    });
                }

                continue;
            }

            std::error_code targetStatusError;
            const std::filesystem::file_status targetStatus =
                entry.status(targetStatusError);

            if (targetStatusError)
            {
                frame.node->scanComplete = false;
                if (errorCallback)
                {
                    errorCallback({
                        entry.path(),
                        targetStatusError,
                        targetStatusError.message()
                    });
                }

                continue;
            }

            const bool isSymlink =
                std::filesystem::is_symlink(linkStatus);

            if (std::filesystem::is_directory(targetStatus))
            {
                domain::DirectoryNode* child =
                    frame.node->addChild(entry.path());

                ++processedDirectories;

                // Для обычных директорий открываем отдельный iterator.
                // Symbolic link на директорию только отображается как
                // отдельный узел и не используется для рекурсивного обхода.
                if (!isSymlink)
                {
                    std::error_code childIteratorError;
                    std::filesystem::directory_iterator childIterator(
                        entry.path(),
                        std::filesystem::directory_options::none,
                        childIteratorError);

                    if (childIteratorError)
                    {
                        child->scanComplete = false;
                        if (errorCallback)
                        {
                            errorCallback({
                                entry.path(),
                                childIteratorError,
                                childIteratorError.message()
                            });
                        }
                    }
                    else
                    {
                        stack.push_back({
                            std::move(childIterator),
                            child
                        });
                    }
                }
            }
            else if (std::filesystem::is_regular_file(targetStatus))
            {
                std::error_code fileSizeError;
                const std::uintmax_t fileSize =
                    entry.file_size(fileSizeError);

                if (fileSizeError)
                {
                    frame.node->scanComplete = false;
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

                    frame.node->stats.addFile(
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
        }

        if (cancellationRequested.load())
        {
            if (result.getRoot())
            {
                result.getRoot()->scanComplete = false;
            }
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
