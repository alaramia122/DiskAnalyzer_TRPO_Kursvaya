#include "visualization/models/VisualizationModel.h"

#include <iostream>
#include <string>

namespace
{
    bool require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    domain::DirectoryStatistics statistics;
    statistics.addFile(domain::FileCategory::Documents, 10);
    statistics.addFile(domain::FileCategory::SourceCode, 20);

    statistics.subtree.filesCount = 3;
    statistics.subtree.filesSizeBytes = 70;
    const std::size_t documents =
        static_cast<std::size_t>(domain::FileCategory::Documents);
    const std::size_t source =
        static_cast<std::size_t>(domain::FileCategory::SourceCode);
    statistics.subtree.categories.filesCount[documents] = 1;
    statistics.subtree.categories.sizesBytes[documents] = 30;
    statistics.subtree.categories.filesCount[source] = 2;
    statistics.subtree.categories.sizesBytes[source] = 40;

    visualization::VisualizationModel model;
    const auto direct = model.buildCategoryValues(
        statistics, visualization::StatisticsScope::Direct);
    const auto subtree = model.buildCategoryValues(
        statistics, visualization::StatisticsScope::Subtree);
    const auto total = model.buildCategoryValues(
        statistics, visualization::StatisticsScope::Total);

    bool passed = true;
    passed &= require(direct[documents].filesCount == 1 &&
                      direct[documents].sizeBytes == 10,
                      "direct values are taken from direct statistics");
    passed &= require(subtree[source].filesCount == 2 &&
                      subtree[source].sizeBytes == 40,
                      "subtree values are taken from subtree statistics");
    passed &= require(total[documents].filesCount == 2 &&
                      total[documents].sizeBytes == 40,
                      "total values sum direct and subtree category values");
    passed &= require(total[source].filesCount == 3 &&
                      total[source].sizeBytes == 60,
                      "total values include both scopes for each category");

    if (!passed)
    {
        return 1;
    }

    std::cout << "All visualization model checks passed.\n";
    return 0;
}
