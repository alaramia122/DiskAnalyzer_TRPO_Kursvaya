#pragma once

#include "../application/ApplicationController.h"
#include "../domain/enums/FileCategory.h"
#include "../domain/models/DirectoryNode.h"
#include "../infrastructure/export/ExportManager.h"
#include "../infrastructure/cache/DirectoryCacheManager.h"
#include "../infrastructure/classifier/FileClassifier.h"
#include "../visualization/models/VisualizationModel.h"

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
class QComboBox;
namespace QtCharts { class QChartView; }

namespace ui
{
    /**
     * @brief Главное окно приложения и адаптер между моделями и Qt Widgets.
     *
     * Не выполняет сканирование самостоятельно: делегирует его контроллеру,
     * отображает готовый AnalysisResult и управляет локальным кэшем навигации.
     */
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
        void exportJson();
        void exportTxt();
        void handleProgress(
            const QString& path,
            qulonglong processedFiles,
            qulonglong processedDirectories);
        void handleAnalysisFinished(int status);
        void showSelectedDirectory(QTreeWidgetItem* item);
        void refreshDirectoryContents();

    private:
        void setBusy(bool busy);
        void populateTree();
        void appendDirectory(
            QTreeWidgetItem* parentItem,
            const domain::DirectoryNode* node);
        void showStatistics(const domain::DirectoryNode* node);
        void showDirectoryContents(const std::filesystem::path& directory);
        void clearDirectoryCache();
        void updateChart(const domain::DirectoryNode* node);
        static QString formatBytes(quint64 bytes);
        static QString categoryName(domain::FileCategory category);
        static QString pathToQString(const std::filesystem::path& path);

        // Сервисы окна. Контроллер и classifier должны жить дольше окна.
        application::ApplicationController& controller_;
        infrastructure::ExportManager exportManager_;
        visualization::VisualizationModel visualizationModel_;
        infrastructure::DirectoryCacheManager directoryCache_;
        infrastructure::FileClassifier fileClassifier_;
        // Элементы ввода и управления анализом.
        QLineEdit* rootPathEdit_ = nullptr;
        QPushButton* browseButton_ = nullptr;
        QPushButton* runButton_ = nullptr;
        QPushButton* cancelButton_ = nullptr;
        QPushButton* refreshDirectoryButton_ = nullptr;
        QPushButton* exportJsonButton_ = nullptr;
        QPushButton* exportTxtButton_ = nullptr;
        // Состояние операции и журнал диагностики.
        QProgressBar* progressBar_ = nullptr;
        QLabel* statusLabel_ = nullptr;
        QLabel* currentPathLabel_ = nullptr;
        // Навигация, табличная статистика и визуализация выбранного узла.
        QTreeWidget* directoryTree_ = nullptr;
        QTableWidget* filesTable_ = nullptr;
        QTableWidget* statisticsTable_ = nullptr;
        QComboBox* chartScopeCombo_ = nullptr;
        QtCharts::QChartView* chartView_ = nullptr;
        QPlainTextEdit* errorsLog_ = nullptr;
    };
}
