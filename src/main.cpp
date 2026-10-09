#include "ui/MainWindow.h"

#include "infrastructure/aggregator/DirectoryAggregator.h"
#include "infrastructure/classifier/FileClassifier.h"
#include "infrastructure/scanner/DirectoryScanner.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    infrastructure::FileClassifier classifier;
    infrastructure::DirectoryScanner scanner(classifier);
    infrastructure::DirectoryAggregator aggregator;
    application::AnalysisPipeline pipeline(scanner, aggregator);
    application::ApplicationController controller(pipeline);
    ui::MainWindow window(controller);

    window.show();
    return application.exec();
}
