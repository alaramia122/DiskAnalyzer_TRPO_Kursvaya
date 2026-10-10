#include "VisualizationModel.h"

namespace visualization
{
    std::array<CategoryValue, domain::FileCategoryCount>
        VisualizationModel::buildCategoryValues(
            const domain::DirectoryStatistics& statistics,
            StatisticsScope scope) const
    {
        std::array<CategoryValue, domain::FileCategoryCount> values{};

        for (std::size_t index = 0; index < domain::FileCategoryCount; ++index)
        {
            values[index].category =
                static_cast<domain::FileCategory>(index);

            switch (scope)
            {
            case StatisticsScope::Direct:
                values[index].filesCount =
                    statistics.direct.categories.filesCount[index];
                values[index].sizeBytes =
                    statistics.direct.categories.sizesBytes[index];
                break;

            case StatisticsScope::Subtree:
                values[index].filesCount =
                    statistics.subtree.categories.filesCount[index];
                values[index].sizeBytes =
                    statistics.subtree.categories.sizesBytes[index];
                break;

            case StatisticsScope::Total:
                values[index].filesCount =
                    statistics.direct.categories.filesCount[index] +
                    statistics.subtree.categories.filesCount[index];
                values[index].sizeBytes =
                    statistics.direct.categories.sizesBytes[index] +
                    statistics.subtree.categories.sizesBytes[index];
                break;
            }
        }

        return values;
    }
}
