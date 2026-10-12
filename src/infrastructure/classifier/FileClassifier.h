#pragma once

#include "../../domain/interfaces/IFileClassifier.h"

#include <string>
#include <unordered_map>

namespace infrastructure
{
    /**
     * @brief Классификатор файлов по расширению.
     *
     * Реализует текущую согласованную стратегию extension-based classification.
     * Расширение нормализуется к нижнему регистру.
     */
    class FileClassifier final : public domain::IFileClassifier
    {
    public:
        /**
         * @brief Определяет категорию файла по его расширению.
         * @param file Информация о файле.
         * @return Категория файла. Неизвестные расширения относятся к Other.
         */
        domain::FileCategory classify(
            const domain::FileInfo& file) const override;

    private:
        static const std::unordered_map<std::string, domain::FileCategory>
            extensionMap_;
    };
}
