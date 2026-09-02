#include "mainwindow.h"

#include "applicationcontroller.h"
#include "ui_mainwindow.h"

#include <QPushButton>

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
        [this](bool)
        {
            refreshUiState();
        });

    connect(
        m_controller,
        &ApplicationController::accessStateChanged,
        this,
        [this](MiniCloud::Client::ClientAccessState)
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

void MainWindow::refreshUiState()
{
    const bool connected = m_controller->isConnected();

    const MiniCloud::Client::ClientAccessState accessState = m_controller->accessState();

    const bool authenticating = accessState == MiniCloud::Client::ClientAccessState::Authenticating;

    const bool active = connected && accessState == MiniCloud::Client::ClientAccessState::Active;

    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    ui->activateButton->setEnabled(connected && accessState == MiniCloud::Client::ClientAccessState::Locked);
    ui->hostLineEdit->setEnabled(!connected);
    ui->portSpinBox->setEnabled(!connected);
    ui->productKeyLineEdit->setEnabled(!authenticating && !active);
    ui->deviceIdLineEdit->setEnabled(!authenticating && !active);
    ui->filePage->setEnabled(active);
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
