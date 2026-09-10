#include "mainwindow.h"

#include "applicationcontroller.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

MainWindow::MainWindow(ApplicationController *controller, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_controller(controller)
{
    Q_ASSERT(m_controller != nullptr);

    ui->setupUi(this);
    ui->errorLabel->clear();
    refreshUiState();

    connect(
        ui->connectButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            ui->errorLabel->clear();
            ui->errorLabel->setToolTip(QString());

            const bool accepted = m_controller->connectToServer(ui->hostLineEdit->text(), static_cast<quint16>(ui->portSpinBox->value()));

            if (!accepted)
            {
                refreshUiState();
                ui->statusLabel->setText(tr("Connection failed."));
                ui->errorLabel->setText(tr("Unable to connect to server."));
                return;
            }

            ui->connectButton->setEnabled(false);

            if (!m_controller->isConnected())
            {
                ui->statusLabel->setText(tr("Connecting..."));
            }
        });

    connect(
        ui->disconnectButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            m_userInitiatedDisconnect = true;
            m_controller->disconnectFromServer();
        });

    connect(
        ui->activateButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QString productKey = ui->productKeyLineEdit->text();
            const QString deviceId = ui->deviceIdLineEdit->text();

            if (productKey.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("Product key is required."));
                return;
            }

            if (deviceId.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("Device ID is required."));
                return;
            }

            ui->errorLabel->clear();

            const auto result = m_controller->activate(productKey, deviceId);

            if (result.status != MiniCloud::Client::RequestSendStatus::Accepted)
            {
                ui->errorLabel->setText(tr("Could not start activation."));
            }
        });

    connect(
        m_controller,
        &ApplicationController::connectionStateChanged,
        this,
        [this](bool connected)
        {
            if (!connected)
            {
                const bool showConnectionLost = m_hadActiveFileSession && !m_userInitiatedDisconnect;
                clearFileSessionUi();
                m_hadActiveFileSession = false;
                m_userInitiatedDisconnect = false;

                refreshUiState();

                if (showConnectionLost)
                {
                    ui->errorLabel->setText(tr("Connection lost."));
                }

                return;
            }

            refreshUiState();
        });

    connect(
        m_controller,
        &ApplicationController::accessStateChanged,
        this,
        [this](MiniCloud::Client::ClientAccessState state)
        {
            if (state == MiniCloud::Client::ClientAccessState::Active)
            {
                m_hadActiveFileSession = true;
            }

            refreshUiState();

            if (state == MiniCloud::Client::ClientAccessState::Active)
            {
                m_controller->requestBrowse(QStringLiteral("/"));
            }
        });

    connect(
        m_controller,
        &ApplicationController::browseReceived,
        this,
        [this](const QString &path, const QList<MiniCloud::Protocol::FileEntryData> &entries)
        {
            ui->currentPathLabel->setText(path);
            ui->fileTable->clearSelection();
            ui->fileTable->setRowCount(entries.size());

            for (qsizetype row = 0; row < entries.size(); ++row)
            {
                const MiniCloud::Protocol::FileEntryData &entry = entries.at(row);
                auto *nameItem = new QTableWidgetItem(entry.name);
                nameItem->setData(Qt::UserRole, entry.path);
                nameItem->setData(Qt::UserRole + 1, static_cast<int>(entry.type));

                const QString type = entry.type == MiniCloud::Protocol::FileEntryType::Directory ? tr("Directory") : entry.type == MiniCloud::Protocol::FileEntryType::File ? tr("File")
                                                                                                                                                                            : tr("Unknown");

                const QString modified = entry.lastModifiedUtcMs == 0
                                             ? QStringLiteral("-")
                                             : QDateTime::fromMSecsSinceEpoch(
                                                   static_cast<qint64>(entry.lastModifiedUtcMs), Qt::UTC)
                                                   .toString(Qt::ISODate);

                ui->fileTable->setItem(row, 0, nameItem);
                ui->fileTable->setItem(row, 1, new QTableWidgetItem(type));
                ui->fileTable->setItem(row, 2, new QTableWidgetItem(QString::number(entry.sizeBytes)));
                ui->fileTable->setItem(row, 3, new QTableWidgetItem(modified));
            }

            refreshUiState();
        });

    connect(
        m_controller,
        &ApplicationController::searchReceived,
        this,
        [this](const QString &, const QList<MiniCloud::Protocol::FileEntryData> &entries)
        {
            ui->fileTable->clearSelection();
            ui->fileTable->setRowCount(entries.size());

            for (qsizetype row = 0; row < entries.size(); ++row)
            {
                const MiniCloud::Protocol::FileEntryData &entry = entries.at(row);
                auto *nameItem = new QTableWidgetItem(entry.name);
                nameItem->setData(Qt::UserRole, entry.path);
                nameItem->setData(Qt::UserRole + 1, static_cast<int>(entry.type));

                const QString type = entry.type == MiniCloud::Protocol::FileEntryType::Directory ? tr("Directory") : entry.type == MiniCloud::Protocol::FileEntryType::File ? tr("File")
                                                                                                                                                                            : tr("Unknown");

                const QString modified = entry.lastModifiedUtcMs == 0
                                             ? QStringLiteral("-")
                                             : QDateTime::fromMSecsSinceEpoch(
                                                   static_cast<qint64>(entry.lastModifiedUtcMs), Qt::UTC)
                                                   .toString(Qt::ISODate);

                ui->fileTable->setItem(row, 0, nameItem);
                ui->fileTable->setItem(row, 1, new QTableWidgetItem(type));
                ui->fileTable->setItem(row, 2, new QTableWidgetItem(QString::number(entry.sizeBytes)));
                ui->fileTable->setItem(row, 3, new QTableWidgetItem(modified));
            }

            refreshUiState();
        });

    connect(
        m_controller,
        &ApplicationController::fileOperationCompleted,
        this,
        [this](const QString &)
        {
            m_transferInProgress = false;
            refreshUiState();

            if (!m_refreshCurrentDirectoryAfterMutation)
            {
                return;
            }

            m_refreshCurrentDirectoryAfterMutation = false;
            ui->newDirectoryNameEdit->clear();
            ui->renameNameEdit->clear();
            ui->moveDestinationPathEdit->clear();
            ui->uploadLocalPathEdit->clear();
            m_controller->requestBrowse(ui->currentPathLabel->text());
        });

    connect(
        m_controller,
        &ApplicationController::fileOperationFailed,
        this,
        [this](const QString &message)
        {
            m_transferInProgress = false;
            m_refreshCurrentDirectoryAfterMutation = false;
            refreshUiState();
            ui->errorLabel->setText(message);
        });

    connect(
        m_controller,
        &ApplicationController::browseFailed,
        this,
        [this](const QString &message)
        {
            m_refreshCurrentDirectoryAfterMutation = false;
            ui->errorLabel->setText(message);
        });

    connect(
        m_controller,
        &ApplicationController::uploadProgress,
        this,
        [this](quint64 bytesTransferred, quint64 totalSizeBytes)
        {
            const int progressPercent = totalSizeBytes == 0
                                            ? 100
                                            : static_cast<int>((bytesTransferred * 100) / totalSizeBytes);
            ui->transferProgressBar->setValue(qBound(0, progressPercent, 100));
        });

    connect(
        m_controller,
        &ApplicationController::downloadProgress,
        this,
        [this](quint64 bytesTransferred, quint64 totalSizeBytes)
        {
            const int progressPercent = totalSizeBytes == 0
                                            ? 100
                                            : static_cast<int>((bytesTransferred * 100) / totalSizeBytes);
            ui->transferProgressBar->setValue(qBound(0, progressPercent, 100));
        });

    connect(
        ui->refreshButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            m_controller->requestBrowse(ui->currentPathLabel->text());
        });

    connect(
        ui->createDirectoryButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            m_refreshCurrentDirectoryAfterMutation = m_controller->requestCreateDirectory(
                ui->currentPathLabel->text(), ui->newDirectoryNameEdit->text());
        });

    connect(
        ui->uploadButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QString localFilePath = ui->uploadLocalPathEdit->text();

            if (localFilePath.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("Local file path is required."));
                return;
            }

            ui->errorLabel->clear();
            ui->transferProgressBar->setValue(0);
            const bool requestAccepted = m_controller->requestUpload(
                localFilePath, ui->currentPathLabel->text());

            m_refreshCurrentDirectoryAfterMutation = requestAccepted;
            if (requestAccepted)
            {
                m_transferInProgress = true;
                refreshUiState();
            }
        });

    connect(
        ui->downloadButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QList<QTableWidgetItem *> selectedItems = ui->fileTable->selectedItems();

            if (selectedItems.isEmpty())
            {
                return;
            }

            QTableWidgetItem *nameItem = ui->fileTable->item(selectedItems.constFirst()->row(), 0);

            if (nameItem == nullptr || static_cast<MiniCloud::Protocol::FileEntryType>(
                                         nameItem->data(Qt::UserRole + 1).toInt()) !=
                                         MiniCloud::Protocol::FileEntryType::File)
            {
                return;
            }

            const QString remotePath = nameItem->data(Qt::UserRole).toString();

            if (remotePath.trimmed().isEmpty())
            {
                return;
            }

            const QString localTargetPath = ui->downloadLocalPathEdit->text();

            if (localTargetPath.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("Local download target path is required."));
                return;
            }

            ui->errorLabel->clear();
            ui->transferProgressBar->setValue(0);
            if (m_controller->requestDownload(remotePath, localTargetPath))
            {
                m_transferInProgress = true;
                refreshUiState();
            }
        });

    connect(
        ui->renameButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QList<QTableWidgetItem *> selectedItems = ui->fileTable->selectedItems();

            if (selectedItems.isEmpty())
            {
                return;
            }

            QTableWidgetItem *nameItem = ui->fileTable->item(selectedItems.constFirst()->row(), 0);

            if (nameItem == nullptr)
            {
                return;
            }

            const QString path = nameItem->data(Qt::UserRole).toString();

            if (path.trimmed().isEmpty())
            {
                return;
            }

            const QString newName = ui->renameNameEdit->text();

            if (newName.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("New name is required."));
                return;
            }

            ui->errorLabel->clear();
            m_refreshCurrentDirectoryAfterMutation = m_controller->requestRename(
                path, newName);
        });

    connect(
        ui->moveButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QList<QTableWidgetItem *> selectedItems = ui->fileTable->selectedItems();

            if (selectedItems.isEmpty())
            {
                return;
            }

            QTableWidgetItem *nameItem = ui->fileTable->item(selectedItems.constFirst()->row(), 0);

            if (nameItem == nullptr)
            {
                return;
            }

            const QString sourcePath = nameItem->data(Qt::UserRole).toString();

            if (sourcePath.trimmed().isEmpty())
            {
                return;
            }

            const QString destinationPath = ui->moveDestinationPathEdit->text();

            if (destinationPath.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("Destination path is required."));
                return;
            }

            ui->errorLabel->clear();
            m_refreshCurrentDirectoryAfterMutation = m_controller->requestMove(
                sourcePath, destinationPath);
        });

    connect(
        ui->deleteButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QList<QTableWidgetItem *> selectedItems = ui->fileTable->selectedItems();

            if (selectedItems.isEmpty())
            {
                return;
            }

            QTableWidgetItem *nameItem = ui->fileTable->item(selectedItems.constFirst()->row(), 0);

            if (nameItem == nullptr)
            {
                return;
            }

            const QString path = nameItem->data(Qt::UserRole).toString();

            if (path.trimmed().isEmpty())
            {
                return;
            }

            const QMessageBox::StandardButton confirmation = QMessageBox::question(
                this,
                tr("Confirm deletion"),
                tr("Delete \"%1\"?").arg(nameItem->text()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (confirmation != QMessageBox::Yes)
            {
                return;
            }

            ui->errorLabel->clear();
            m_refreshCurrentDirectoryAfterMutation = m_controller->requestDelete(path);
        });

    connect(
        ui->searchButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QString query = ui->searchEdit->text();

            if (query.trimmed().isEmpty())
            {
                ui->errorLabel->setText(tr("Search query is required."));
                return;
            }

            ui->errorLabel->clear();
            m_controller->requestSearch(ui->currentPathLabel->text(), query);
        });

    connect(
        ui->upButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            const QString currentPath = ui->currentPathLabel->text();

            if (currentPath == QStringLiteral("/"))
            {
                return;
            }

            const int separatorIndex = currentPath.lastIndexOf(QLatin1Char('/'));
            const QString parentPath = separatorIndex <= 0 ? QStringLiteral("/") : currentPath.left(separatorIndex);
            m_controller->requestBrowse(parentPath);
        });

    connect(
        ui->fileTable,
        &QTableWidget::itemDoubleClicked,
        this,
        [this](QTableWidgetItem *item)
        {
            if (item == nullptr)
            {
                return;
            }

            QTableWidgetItem *nameItem = ui->fileTable->item(item->row(), 0);

            if (nameItem == nullptr)
            {
                return;
            }

            const auto type = static_cast<MiniCloud::Protocol::FileEntryType>(
                nameItem->data(Qt::UserRole + 1).toInt());

            if (type != MiniCloud::Protocol::FileEntryType::Directory)
            {
                return;
            }

            const QString path = nameItem->data(Qt::UserRole).toString();

            if (path.trimmed().isEmpty())
            {
                return;
            }

            m_controller->requestBrowse(path);
        });

    connect(
        ui->fileTable,
        &QTableWidget::itemSelectionChanged,
        this,
        [this]()
        {
            refreshUiState();
        });

    connect(
        m_controller,
        &ApplicationController::connectionFailed,
        this,
        [this](const QString &technicalMessage)
        {
            refreshUiState();
            ui->statusLabel->setText(tr("Connection failed."));
            ui->errorLabel->setText(tr("Unable to connect to server."));
            ui->errorLabel->setToolTip(technicalMessage);
        });

    connect(
        m_controller,
        &ApplicationController::activationRejected,
        this,
        [this](MiniCloud::Protocol::AuthenticationStatus status)
        {
            switch (status)
            {
            case MiniCloud::Protocol::AuthenticationStatus::InvalidKey:
                ui->errorLabel->setText(tr("Invalid product key."));
                break;

            case MiniCloud::Protocol::AuthenticationStatus::Disabled:
                ui->errorLabel->setText(tr("This license is disabled."));
                break;

            case MiniCloud::Protocol::AuthenticationStatus::DeviceMismatch:
                ui->errorLabel->setText(tr("This license is bound to another device."));
                break;

            default:
                ui->errorLabel->setText(tr("Activation was rejected."));
                break;
            }
        });

    connect(
        m_controller,
        &ApplicationController::activationError,
        this,
        [this](const MiniCloud::Protocol::ErrorResponseData &error)
        {
            if (!error.message.trimmed().isEmpty())
            {
                ui->errorLabel->setText(error.message);
                return;
            }

            ui->errorLabel->setText(
                tr("The server rejected the request."));
        });

    connect(
        m_controller,
        &ApplicationController::activationFailed,
        this,
        [this](MiniCloud::Client::RequestDispatchError error)
        {
            switch (error)
            {
            case MiniCloud::Client::RequestDispatchError::InvalidResponsePayload:
                ui->errorLabel->setText(tr("Invalid activation response."));
                break;

            case MiniCloud::Client::RequestDispatchError::RequestTimeout:
                ui->errorLabel->setText(tr("Activation request timed out."));
                break;

            case MiniCloud::Client::RequestDispatchError::ConnectionLost:
                ui->errorLabel->setText(tr("Connection lost."));
                break;

            default:
                ui->errorLabel->setText(tr("Activation failed."));
                break;
            }
        });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::clearFileSessionUi()
{
    m_refreshCurrentDirectoryAfterMutation = false;
    m_transferInProgress = false;
    ui->currentPathLabel->setText(QStringLiteral("/"));
    ui->fileTable->clearSelection();
    ui->fileTable->clearContents();
    ui->fileTable->setRowCount(0);
    ui->newDirectoryNameEdit->clear();
    ui->searchEdit->clear();
    ui->uploadLocalPathEdit->clear();
    ui->downloadLocalPathEdit->clear();
    ui->renameNameEdit->clear();
    ui->moveDestinationPathEdit->clear();
    ui->transferProgressBar->setValue(0);
    ui->errorLabel->clear();
}

void MainWindow::refreshUiState()
{
    const bool connected = m_controller->isConnected();

    const MiniCloud::Client::ClientAccessState accessState = m_controller->accessState();

    const bool authenticating = accessState == MiniCloud::Client::ClientAccessState::Authenticating;

    const bool active = connected && accessState == MiniCloud::Client::ClientAccessState::Active;
    const bool fileActionsEnabled = active && !m_transferInProgress;

    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    ui->activateButton->setEnabled(connected && accessState == MiniCloud::Client::ClientAccessState::Locked);
    ui->hostLineEdit->setEnabled(!connected);
    ui->portSpinBox->setEnabled(!connected);
    ui->productKeyLineEdit->setEnabled(!authenticating && !active);
    ui->deviceIdLineEdit->setEnabled(!authenticating && !active);
    ui->filePage->setEnabled(fileActionsEnabled);
    ui->refreshButton->setEnabled(fileActionsEnabled);
    ui->upButton->setEnabled(fileActionsEnabled && ui->currentPathLabel->text() != QStringLiteral("/"));
    ui->newDirectoryNameEdit->setEnabled(fileActionsEnabled);
    ui->createDirectoryButton->setEnabled(fileActionsEnabled);
    ui->searchEdit->setEnabled(fileActionsEnabled);
    ui->searchButton->setEnabled(fileActionsEnabled);
    ui->uploadLocalPathEdit->setEnabled(fileActionsEnabled);
    ui->uploadButton->setEnabled(fileActionsEnabled);
    ui->downloadLocalPathEdit->setEnabled(fileActionsEnabled);

    bool hasSelectedFile = false;
    const QList<QTableWidgetItem *> selectedItems = ui->fileTable->selectedItems();
    if (!selectedItems.isEmpty())
    {
        QTableWidgetItem *nameItem = ui->fileTable->item(selectedItems.constFirst()->row(), 0);
        hasSelectedFile = nameItem != nullptr && static_cast<MiniCloud::Protocol::FileEntryType>(
                                                   nameItem->data(Qt::UserRole + 1).toInt()) ==
                                                   MiniCloud::Protocol::FileEntryType::File;
    }

    ui->downloadButton->setEnabled(fileActionsEnabled && hasSelectedFile);
    ui->renameNameEdit->setEnabled(fileActionsEnabled);
    ui->renameButton->setEnabled(fileActionsEnabled && !ui->fileTable->selectedItems().isEmpty());
    ui->moveDestinationPathEdit->setEnabled(fileActionsEnabled);
    ui->moveButton->setEnabled(fileActionsEnabled && !ui->fileTable->selectedItems().isEmpty());
    ui->deleteButton->setEnabled(fileActionsEnabled && !ui->fileTable->selectedItems().isEmpty());
    ui->mainStackedWidget->setCurrentWidget(active ? ui->filePage : ui->activationPage);

    if (active)
    {
        ui->statusLabel->setText(tr("Activated"));
        ui->errorLabel->clear();
    }
    else if (authenticating)
    {
        ui->statusLabel->setText(tr("Authenticating..."));
    }
    else
    {
        ui->statusLabel->setText(connected ? tr("Connected") : tr("Disconnected"));
    }
}
