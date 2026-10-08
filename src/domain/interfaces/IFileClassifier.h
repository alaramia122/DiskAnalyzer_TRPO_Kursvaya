#pragma once

#include "../models/FileInfo.h"
#include "../enums/FileCategory.h"

namespace domain
{
    /**
     * @brief Интерфейс классификатора файлов.
     *
     * Классификатор получает уже сформированный FileInfo и определяет
     * категорию файла. Интерфейс отделяет Domain от конкретного алгоритма
     * классификации и позволяет подменять реализацию при тестировании.
     */
    class IFileClassifier
    {
    public:
        virtual ~IFileClassifier() = default;

        /**
         * @brief Определяет категорию файла.
         * @param file Информация о файле.
         * @return Категория файла.
         */
        virtual FileCategory classify(const FileInfo& file) const = 0;
    };
}
