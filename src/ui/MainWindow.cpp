#include "MainWindow.h"

#include "../domain/enums/ScanStatus.h"
#include "../domain/models/DirectoryNode.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <cstdint>

namespace ui
{
    namespace
    {
        constexpr int PathRole = Qt::UserRole + 1;

        const std::array<domain::FileCategory, domain::FileCategoryCount>
            Categories = {
                domain::FileCategory::Unknown,
                domain::FileCategory::Documents,
                domain::FileCategory::Images,
                domain::FileCategory::Video,
                domain::FileCategory::Audio,
                domain::FileCategory::Archives,
                domain::FileCategory::Executables,
                domain::FileCategory::SourceCode,
                domain::FileCategory::System,
                domain::FileCategory::Other
            };
    }

    MainWindow::MainWindow(
        application::ApplicationController& controller,
        QWidget* parent)
        : QMainWindow(parent),
          controller_(controller)
    {
        setWindowTitle(QStringLiteral("Disk Analyzer"));
        resize(1200, 800);

        auto* central = new QWidget(this);
        auto* mainLayout = new QVBoxLayout(central);

        auto* pathLayout = new QHBoxLayout();
        rootPathEdit_ = new QLineEdit(central);
        rootPathEdit_->setPlaceholderText(
            QStringLiteral("Выберите каталог для анализа"));
        browseButton_ = new QPushButton(QStringLiteral("Обзор…"), central);
        runButton_ = new QPushButton(QStringLiteral("Начать анализ"), central);
        cancelButton_ = new QPushButton(QStringLiteral("Отмена"), central);
        cancelButton_->setEnabled(false);
        pathLayout->addWidget(new QLabel(QStringLiteral("Каталог:"), central));
        pathLayout->addWidget(rootPathEdit_, 1);
        pathLayout->addWidget(browseButton_);
        pathLayout->addWidget(runButton_);
        pathLayout->addWidget(cancelButton_);
        mainLayout->addLayout(pathLayout);

        statusLabel_ = new QLabel(QStringLiteral("Готов к анализу."), central);
        currentPathLabel_ = new QLabel(central);
        currentPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        progressBar_ = new QProgressBar(central);
        progressBar_->setRange(0, 0);
        progressBar_->setVisible(false);
        mainLayout->addWidget(statusLabel_);
        mainLayout->addWidget(currentPathLabel_);
        mainLayout->addWidget(progressBar_);

        auto* splitter = new QSplitter(Qt::Horizontal, central);
        directoryTree_ = new QTreeWidget(splitter);
        directoryTree_->setHeaderLabels({QStringLiteral("Каталоги")});
        directoryTree_->setMinimumWidth(280);

        auto* rightPanel = new QWidget(splitter);
        auto* rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->addWidget(new QLabel(
            QStringLiteral("Статистика выбранного каталога"), rightPanel));

        statisticsTable_ = new QTableWidget(0, 5, rightPanel);
        statisticsTable_->setHorizontalHeaderLabels({
            QStringLiteral("Категория"),
            QStringLiteral("Файлы здесь"),
            QStringLiteral("Размер здесь"),
            QStringLiteral("Файлы в поддереве"),
            QStringLiteral("Размер поддерева")
        });
        statisticsTable_->horizontalHeader()->setSectionResizeMode(
            QHeaderView::ResizeToContents);
        statisticsTable_->horizontalHeader()->setStretchLastSection(true);
        statisticsTable_->verticalHeader()->setVisible(false);
        statisticsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        statisticsTable_->setSelectionMode(QAbstractItemView::NoSelection);
        rightLayout->addWidget(statisticsTable_, 1);

        rightLayout->addWidget(new QLabel(
            QStringLiteral("Ошибки доступа к файловой системе"), rightPanel));
        errorsLog_ = new QPlainTextEdit(rightPanel);
        errorsLog_->setReadOnly(true);
        errorsLog_->setMaximumBlockCount(2000);
        errorsLog_->setPlaceholderText(
            QStringLiteral("Ошибки чтения каталогов и файлов появятся здесь."));
        rightLayout->addWidget(errorsLog_, 1);

        splitter->addWidget(directoryTree_);
        splitter->addWidget(rightPanel);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 2);
        mainLayout->addWidget(splitter, 1);
        setCentralWidget(central);

        connect(browseButton_, &QPushButton::clicked,
                this, &MainWindow::chooseRootDirectory);
        connect(runButton_, &QPushButton::clicked,
                this, &MainWindow::startAnalysis);
        connect(cancelButton_, &QPushButton::clicked,
                this, &MainWindow::cancelAnalysis);
        connect(directoryTree_, &QTreeWidget::currentItemChanged,
                this, &MainWindow::showSelectedDirectory);

        connect(&controller_, &application::ApplicationController::analysisStarted,
                this, [this]()
                {
                    statusLabel_->setText(QStringLiteral("Анализ выполняется…"));
                    errorsLog_->clear();
                    directoryTree_->clear();
                    statisticsTable_->setRowCount(0);
                });
        connect(&controller_, &application::ApplicationController::progressChanged,
                this, &MainWindow::handleProgress);
        connect(&controller_, &application::ApplicationController::scanError,
                this, [this](const QString& path, const QString& message)
                {
                    errorsLog_->appendPlainText(path + QStringLiteral(": ") + message);
                });
        connect(&controller_, &application::ApplicationController::analysisFinished,
                this, &MainWindow::handleAnalysisFinished);
    }

    void MainWindow::chooseRootDirectory()
    {
        const QString selected = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Выберите каталог для анализа"),
            rootPathEdit_->text());
        if (!selected.isEmpty())
        {
            rootPathEdit_->setText(selected);
        }
    }

    void MainWindow::startAnalysis()
    {
        const QString selectedPath = rootPathEdit_->text().trimmed();
        if (selectedPath.isEmpty())
        {
            QMessageBox::information(
                this, QStringLiteral("Каталог не выбран"),
                QStringLiteral("Укажите каталог, который нужно проанализировать."));
            return;
        }

        const QByteArray encodedPath = selectedPath.toUtf8();
        const std::filesystem::path path = std::filesystem::u8path(encodedPath.constData());
        if (!controller_.startAnalysis(path))
        {
            statusLabel_->setText(
                QStringLiteral("Предыдущая задача ещё завершается. Повторите запуск через секунду."));
            return;
        }

        setBusy(true);
        statusLabel_->setText(QStringLiteral("Подготовка к анализу…"));
        currentPathLabel_->clear();
    }

    void MainWindow::cancelAnalysis()
    {
        controller_.cancelAnalysis();
        cancelButton_->setEnabled(false);
        statusLabel_->setText(QStringLiteral("Запрошена отмена анализа…"));
    }

    void MainWindow::handleProgress(
        const QString& path,
        qulonglong processedFiles,
        qulonglong processedDirectories)
    {
        currentPathLabel_->setText(
            QStringLiteral("Обработано файлов: %1; каталогов: %2\n%3")
                .arg(processedFiles)
                .arg(processedDirectories)
                .arg(path));
    }

    void MainWindow::handleAnalysisFinished(int statusValue)
    {
        setBusy(false);
        const auto status = static_cast<domain::ScanStatus>(statusValue);
        switch (status)
        {
        case domain::ScanStatus::Completed:
            statusLabel_->setText(QStringLiteral("Анализ завершён."));
            populateTree();
            break;
        case domain::ScanStatus::Cancelled:
            statusLabel_->setText(QStringLiteral("Анализ отменён."));
            populateTree();
            break;
        case domain::ScanStatus::Failed:
            statusLabel_->setText(QStringLiteral("Не удалось выполнить анализ. Проверьте путь и журнал ошибок."));
            populateTree();
            break;
        case domain::ScanStatus::Idle:
        case domain::ScanStatus::Running:
            statusLabel_->setText(QStringLiteral("Получен некорректный итоговый статус."));
            break;
        }
    }

    void MainWindow::setBusy(bool busy)
    {
        rootPathEdit_->setEnabled(!busy);
        browseButton_->setEnabled(!busy);
        runButton_->setEnabled(!busy);
        cancelButton_->setEnabled(busy);
        progressBar_->setVisible(busy);
    }

    void MainWindow::populateTree()
    {
        directoryTree_->clear();
        const domain::DirectoryNode* root = controller_.result().getRoot();
        if (!root)
        {
            return;
        }

        appendDirectory(nullptr, root);
        if (directoryTree_->topLevelItemCount() > 0)
        {
            directoryTree_->setCurrentItem(directoryTree_->topLevelItem(0));
            directoryTree_->expandToDepth(1);
        }
    }

    void MainWindow::appendDirectory(
        QTreeWidgetItem* parentItem,
        const domain::DirectoryNode* node)
    {
        if (!node)
        {
            return;
        }

        const QString path = pathToQString(node->path);
        const QString name = node->path.filename().empty()
            ? path : pathToQString(node->path.filename());
        auto* item = parentItem
            ? new QTreeWidgetItem(parentItem)
            : new QTreeWidgetItem(directoryTree_);
        item->setText(0, name);
        item->setToolTip(0, path);
        item->setData(0, PathRole, path);
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(
            static_cast<qulonglong>(reinterpret_cast<quintptr>(node))));

        for (const auto& child : node->children)
        {
            appendDirectory(item, child.get());
        }
    }

    void MainWindow::showSelectedDirectory(QTreeWidgetItem* item)
    {
        if (!item)
        {
            statisticsTable_->setRowCount(0);
            return;
        }

        const auto address = item->data(0, Qt::UserRole).toULongLong();
        const auto* node = reinterpret_cast<const domain::DirectoryNode*>(
            static_cast<quintptr>(address));
        showStatistics(node);
    }

    void MainWindow::showStatistics(const domain::DirectoryNode* node)
    {
        statisticsTable_->setRowCount(0);
        if (!node)
        {
            return;
        }

        for (const auto category : Categories)
        {
            const auto index = static_cast<std::size_t>(category);
            const auto& direct = node->stats.direct;
            const auto& subtree = node->stats.subtree;
            const quint64 directCount = direct.categories.filesCount[index];
            const quint64 subtreeCount = subtree.categories.filesCount[index];
            if (directCount == 0 && subtreeCount == 0)
            {
                continue;
            }

            const int row = statisticsTable_->rowCount();
            statisticsTable_->insertRow(row);
            const QStringList values = {
                categoryName(category),
                QString::number(directCount),
                formatBytes(direct.categories.sizesBytes[index]),
                QString::number(subtreeCount),
                formatBytes(subtree.categories.sizesBytes[index])
            };
            for (int column = 0; column < values.size(); ++column)
            {
                statisticsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
            }
        }

        const int totalRow = statisticsTable_->rowCount();
        statisticsTable_->insertRow(totalRow);
        const QStringList totals = {
            QStringLiteral("Всего"),
            QString::number(node->stats.direct.filesCount),
            formatBytes(node->stats.direct.filesSizeBytes),
            QString::number(node->stats.subtree.filesCount),
            formatBytes(node->stats.subtree.filesSizeBytes)
        };
        for (int column = 0; column < totals.size(); ++column)
        {
            auto* item = new QTableWidgetItem(totals[column]);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            statisticsTable_->setItem(totalRow, column, item);
        }
    }

    QString MainWindow::formatBytes(quint64 bytes)
    {
        static const char* units[] = {"Б", "КиБ", "МиБ", "ГиБ", "ТиБ"};
        double value = static_cast<double>(bytes);
        int unit = 0;
        while (value >= 1024.0 && unit < 4)
        {
            value /= 1024.0;
            ++unit;
        }
        return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 2)
            .arg(QString::fromUtf8(units[unit]));
    }

    QString MainWindow::categoryName(domain::FileCategory category)
    {
        switch (category)
        {
        case domain::FileCategory::Unknown: return QStringLiteral("Неизвестно");
        case domain::FileCategory::Documents: return QStringLiteral("Документы");
        case domain::FileCategory::Images: return QStringLiteral("Изображения");
        case domain::FileCategory::Video: return QStringLiteral("Видео");
        case domain::FileCategory::Audio: return QStringLiteral("Аудио");
        case domain::FileCategory::Archives: return QStringLiteral("Архивы");
        case domain::FileCategory::Executables: return QStringLiteral("Исполняемые файлы");
        case domain::FileCategory::SourceCode: return QStringLiteral("Исходный код");
        case domain::FileCategory::System: return QStringLiteral("Системные");
        case domain::FileCategory::Other: return QStringLiteral("Прочие");
        }
        return QStringLiteral("Неизвестно");
    }

    QString MainWindow::pathToQString(const std::filesystem::path& path)
    {
        return QString::fromUtf8(path.u8string().c_str());
    }
}
