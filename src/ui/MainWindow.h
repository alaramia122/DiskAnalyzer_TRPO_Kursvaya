#pragma once

#include "../application/ApplicationController.h"

#include <QMainWindow>
#include <QString>
#include <filesystem>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;

namespace ui
{
    class MainWindow final : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(
            application::ApplicationController& controller,
            QWidget* parent = nullptr);

    private slots:
        void chooseRootDirectory();
        void startAnalysis();
        void cancelAnalysis();
        void handleProgress(
            const QString& path,
            qulonglong processedFiles,
            qulonglong processedDirectories);
        void handleAnalysisFinished(int status);
        void showSelectedDirectory(QTreeWidgetItem* item);

    private:
        void setBusy(bool busy);
        void populateTree();
        void appendDirectory(
            QTreeWidgetItem* parentItem,
            const domain::DirectoryNode* node);
        void showStatistics(const domain::DirectoryNode* node);
        static QString formatBytes(quint64 bytes);
        static QString categoryName(domain::FileCategory category);
        static QString pathToQString(const std::filesystem::path& path);

        application::ApplicationController& controller_;
        QLineEdit* rootPathEdit_ = nullptr;
        QPushButton* browseButton_ = nullptr;
        QPushButton* runButton_ = nullptr;
        QPushButton* cancelButton_ = nullptr;
        QProgressBar* progressBar_ = nullptr;
        QLabel* statusLabel_ = nullptr;
        QLabel* currentPathLabel_ = nullptr;
        QTreeWidget* directoryTree_ = nullptr;
        QTableWidget* statisticsTable_ = nullptr;
        QPlainTextEdit* errorsLog_ = nullptr;
    };
}
