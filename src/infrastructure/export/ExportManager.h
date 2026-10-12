#pragma once

#include "../../domain/enums/ExportFormat.h"
#include "../../domain/models/AnalysisResult.h"

#include <filesystem>
#include <string>

namespace infrastructure
{
    /**
     * @brief Экспортирует агрегированные результаты анализа.
     *
     * Экспортирует только дерево каталогов и статистику; сведения об отдельных
     * файлах не сохраняются в AnalysisResult и не восстанавливаются из диска.
     */
    class ExportManager final
    {
    public:
        /**
         * @brief Записывает результат анализа в файл.
         * @param result Результат завершённого или частичного анализа.
         * @param outputPath Путь выходного файла.
         * @param format Формат JSON или TXT.
         * @param errorMessage Необязательный текст ошибки записи.
         * @return true, если файл полностью записан; иначе false.
         *
         * При ошибке открытия или записи возвращает false и, если передан
         * errorMessage, заполняет его описанием ошибки.
         */
        bool exportResult(
            const domain::AnalysisResult& result,
            const std::filesystem::path& outputPath,
            domain::ExportFormat format,
            std::string* errorMessage = nullptr) const;
    };
}
