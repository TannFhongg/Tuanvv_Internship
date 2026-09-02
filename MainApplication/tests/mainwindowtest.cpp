#include <QtTest/QtTest>

#include <QLabel>
#include <QLineEdit>
#include <QJsonObject>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>

#include "applicationcontroller.h"
#include "authentication.h"
#include "errorresponse.h"
#include "frameparser.h"
#include "mainwindow.h"
#include "protocolcodec.h"
#include "protocoltypes.h"

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void initialState_showsActivationPageAndDisablesFileFeatures()
    {
        ApplicationController controller;
        MainWindow window(&controller);

        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(activationPage->isEnabled());
        QVERIFY(!filePage->isEnabled());
        QVERIFY(hostEdit->isEnabled());
        QVERIFY(portSpinBox->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QVERIFY(connectButton->isEnabled());
        QVERIFY(!disconnectButton->isEnabled());
        QVERIFY(!activateButton->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(errorLabel->text().isEmpty());

        QCOMPARE(controller.accessState(), MiniCloud::Client::ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
    }

    void connectButton_validEndpoint_connectsAndEnablesActivationControls()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        const QString initialStatus = statusLabel->text();
        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());

        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QCOMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(!connectButton->isEnabled());
        QTRY_VERIFY(disconnectButton->isEnabled());
        QTRY_VERIFY(activateButton->isEnabled());
        QVERIFY(!hostEdit->isEnabled());
        QVERIFY(!portSpinBox->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != initialStatus);
        QVERIFY(errorLabel->text().isEmpty());

        QCOMPARE(controller.accessState(), MiniCloud::Client::ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
    }

    void disconnectButton_whenConnected_disconnectsAndRestoresLockedUi()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(disconnectButton->isEnabled());
        const QString connectedStatus = statusLabel->text();
        QTest::mouseClick(disconnectButton, Qt::LeftButton);

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::UnconnectedState);
        QTRY_VERIFY(connectButton->isEnabled());
        QTRY_VERIFY(!disconnectButton->isEnabled());
        QTRY_VERIFY(!activateButton->isEnabled());
        QVERIFY(hostEdit->isEnabled());
        QVERIFY(portSpinBox->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != connectedStatus);
        QVERIFY(errorLabel->text().isEmpty());

        QCOMPARE(controller.accessState(), MiniCloud::Client::ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
    }

    void activateButton_validCredentials_sendsAuthenticationAndShowsPendingState()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticateRequestDecodeResult;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::RequestId;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(productKey);
        deviceIdEdit->setText(deviceId);
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_VERIFY(activateButton->isEnabled());

        const QString connectedStatus = statusLabel->text();
        QTest::mouseClick(activateButton, Qt::LeftButton);

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(!activateButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(!productKeyEdit->isEnabled());
        QVERIFY(!deviceIdEdit->isEnabled());
        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != connectedStatus);
        QVERIFY(errorLabel->text().isEmpty());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto parsed = parser.tryTakeFrame();

        QCOMPARE(parsed.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsed.frame.header.messageType, MessageType::AuthenticateRequest);
        QVERIFY(parsed.frame.header.requestId != RequestId{0});
        QCOMPARE(parsed.frame.header.taskId, TaskId{0});

        const auto decoded =
            MiniCloud::Protocol::deserializeAuthenticateRequest(parsed.frame.payload);
        QCOMPARE(decoded.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(decoded.data.productKey, productKey);
        QCOMPARE(decoded.data.deviceId, deviceId);
    }

    void authenticateResponse_valid_opensFileFeaturesAndKeepsSessionConnected()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(QStringLiteral("MCLD-1111-2222-3333-4444"));
        deviceIdEdit->setText(QStringLiteral("DEVICE-CLIENT"));
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);
        const QString authenticatingStatus = statusLabel->text();

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto requestFrame = parser.tryTakeFrame();
        QCOMPARE(requestFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrame.frame.header.messageType, MessageType::AuthenticateRequest);

        const auto responsePayload = MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(responsePayload.status, AuthenticationEncodeResult::Status::Success);

        const auto responseFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            requestFrame.frame.header.requestId,
            TaskId{0},
            responsePayload.payload);
        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(responseFrame.encodedFrame), static_cast<qint64>(responseFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());
        QTRY_COMPARE(stack->currentWidget(), filePage);
        QTRY_VERIFY(filePage->isEnabled());
        QVERIFY(!connectButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(!activateButton->isEnabled());
        QVERIFY(!productKeyEdit->isEnabled());
        QVERIFY(!deviceIdEdit->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != authenticatingStatus);
        QVERIFY(errorLabel->text().isEmpty());
        QVERIFY(activationPage != stack->currentWidget());
    }

    void authenticateResponse_nonValidStatus_staysLockedShowsErrorAndAllowsRetry_data()
    {
        using MiniCloud::Protocol::AuthenticationStatus;

        QTest::addColumn<AuthenticationStatus>("authenticationStatus");
        QTest::addColumn<QString>("expectedError");

        QTest::newRow("invalid-key") << AuthenticationStatus::InvalidKey << QStringLiteral("Invalid product key.");
        QTest::newRow("disabled") << AuthenticationStatus::Disabled << QStringLiteral("This license is disabled.");
        QTest::newRow("device-mismatch") << AuthenticationStatus::DeviceMismatch << QStringLiteral("This license is bound to another device.");
    }

    void authenticateResponse_nonValidStatus_staysLockedShowsErrorAndAllowsRetry()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(AuthenticationStatus, authenticationStatus);
        QFETCH(QString, expectedError);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(QStringLiteral("MCLD-1111-2222-3333-4444"));
        deviceIdEdit->setText(QStringLiteral("DEVICE-CLIENT"));
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);
        const QString authenticatingStatus = statusLabel->text();

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto requestFrame = parser.tryTakeFrame();
        QCOMPARE(requestFrame.status, FrameParser::FrameParseStatus::FrameReady);

        const auto responsePayload = MiniCloud::Protocol::serializeAuthenticateResponse({authenticationStatus});
        QCOMPARE(responsePayload.status, AuthenticationEncodeResult::Status::Success);

        const auto responseFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            requestFrame.frame.header.requestId,
            TaskId{0},
            responsePayload.payload);

        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(responseFrame.encodedFrame), static_cast<qint64>(responseFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(controller.isConnected());
        QVERIFY(!controller.isFeatureAccessAllowed());
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(!connectButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(activateButton->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != authenticatingStatus);
        QTRY_COMPARE(errorLabel->text(), expectedError);
    }

    void errorResponse_duringAuthentication_showsRemoteErrorAndAllowsRetry()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::ErrorCode;
        using MiniCloud::Protocol::ErrorResponseData;
        using MiniCloud::Protocol::ErrorResponseEncodeResult;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(QStringLiteral("MCLD-1111-2222-3333-4444"));
        deviceIdEdit->setText(QStringLiteral("DEVICE-CLIENT"));
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);
        const QString authenticatingStatus = statusLabel->text();

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto requestFrame = parser.tryTakeFrame();
        QCOMPARE(requestFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrame.frame.header.messageType, MessageType::AuthenticateRequest);

        const ErrorResponseData errorData{
            ErrorCode::InternalServerError,
            QStringLiteral("Authentication could not be completed."),
            QJsonObject{}};

        const auto errorPayload = MiniCloud::Protocol::serializeErrorResponse(errorData);
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);

        const auto errorFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::ErrorResponse,
            requestFrame.frame.header.requestId,
            TaskId{0},
            errorPayload.payload);

        QCOMPARE(errorFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(errorFrame.encodedFrame), static_cast<qint64>(errorFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(controller.isConnected());
        QVERIFY(!controller.isFeatureAccessAllowed());
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(!connectButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(activateButton->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != authenticatingStatus);
        QTRY_COMPARE(errorLabel->text(), errorData.message);
    }

    void authenticateResponse_malformedPayload_showsInvalidResponseErrorAndAllowsRetry()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(QStringLiteral("MCLD-1111-2222-3333-4444"));
        deviceIdEdit->setText(QStringLiteral("DEVICE-CLIENT"));
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto requestFrame = parser.tryTakeFrame();
        QCOMPARE(requestFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrame.frame.header.messageType, MessageType::AuthenticateRequest);

        const QByteArray malformedPayload = QByteArrayLiteral("{not-valid-auth-response");

        const auto responseFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            requestFrame.frame.header.requestId,
            TaskId{0},
            malformedPayload);

        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(responseFrame.encodedFrame), static_cast<qint64>(responseFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(controller.isConnected());
        QVERIFY(!controller.isFeatureAccessAllowed());
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(!connectButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(activateButton->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QTRY_COMPARE(errorLabel->text(), QStringLiteral("Invalid activation response."));
    }

    void authenticationRequest_whenDeadlineExpires_showsTimeoutAndAllowsRetry()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller(100);
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(QStringLiteral("MCLD-1111-2222-3333-4444"));
        deviceIdEdit->setText(QStringLiteral("DEVICE-CLIENT"));
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);
        QVERIFY(!activateButton->isEnabled());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto requestFrame = parser.tryTakeFrame();
        QCOMPARE(requestFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrame.frame.header.messageType, MessageType::AuthenticateRequest);

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QTRY_COMPARE(errorLabel->text(), QStringLiteral("Activation request timed out."));

        QVERIFY(controller.isConnected());
        QVERIFY(!controller.isFeatureAccessAllowed());
        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(!connectButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(activateButton->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
    }

    void disconnectButton_afterActivation_closesFileFeaturesAndReturnsToActivationPage()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(QStringLiteral("MCLD-1111-2222-3333-4444"));
        deviceIdEdit->setText(QStringLiteral("DEVICE-CLIENT"));
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto requestFrame = parser.tryTakeFrame();
        QCOMPARE(requestFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrame.frame.header.messageType, MessageType::AuthenticateRequest);

        const auto responsePayload = MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(responsePayload.status, AuthenticationEncodeResult::Status::Success);

        const auto responseFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            requestFrame.frame.header.requestId,
            TaskId{0},
            responsePayload.payload);

        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(responseFrame.encodedFrame), static_cast<qint64>(responseFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(stack->currentWidget(), filePage);
        QTRY_VERIFY(filePage->isEnabled());
        const QString activeStatus = statusLabel->text();

        QTest::mouseClick(disconnectButton, Qt::LeftButton);

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(connectButton->isEnabled());
        QVERIFY(!disconnectButton->isEnabled());
        QVERIFY(!activateButton->isEnabled());
        QVERIFY(hostEdit->isEnabled());
        QVERIFY(portSpinBox->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != activeStatus);
        QVERIFY(errorLabel->text().isEmpty());
    }

    void activateButton_blankRequiredField_showsValidationErrorWithoutSendingFrame_data()
    {
        QTest::addColumn<QString>("productKey");
        QTest::addColumn<QString>("deviceId");
        QTest::addColumn<QString>("expectedError");

        const QString validKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString validDevice = QStringLiteral("DEVICE-CLIENT");

        QTest::newRow("empty-product-key") << QString() << validDevice << QStringLiteral("Product key is required.");
        QTest::newRow("blank-product-key") << QStringLiteral("   ") << validDevice << QStringLiteral("Product key is required.");
        QTest::newRow("empty-device-id") << validKey << QString() << QStringLiteral("Device ID is required.");
        QTest::newRow("blank-device-id") << validKey << QStringLiteral("   ") << QStringLiteral("Device ID is required.");
    }

    void activateButton_blankRequiredField_showsValidationErrorWithoutSendingFrame()
    {
        using MiniCloud::Client::ClientAccessState;

        QFETCH(QString, productKey);
        QFETCH(QString, deviceId);
        QFETCH(QString, expectedError);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        productKeyEdit->setText(productKey);
        deviceIdEdit->setText(deviceId);

        QSignalSpy readyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(readyReadSpy.isValid());

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(controller.isConnected());
        QVERIFY(!controller.isFeatureAccessAllowed());
        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(activateButton->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QCOMPARE(errorLabel->text(), expectedError);

        QTest::qWait(50);
        QCOMPARE(readyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
    }

    void activate_afterInvalidKey_retriesWithEditedKeyAndOpensFilePageWithoutReconnect()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticateRequestDecodeResult;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        const QString invalidKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString correctedKey = QStringLiteral("MCLD-9999-8888-7777-6666");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        QSignalSpy connectionSpy(&controller, &ApplicationController::connectionStateChanged);
        QVERIFY(connectionSpy.isValid());

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(errorLabel != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(invalidKey);
        deviceIdEdit->setText(deviceId);
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(connectionSpy.count(), 1);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser firstParser;
        firstParser.appendData(serverSocket->readAll());
        const auto firstRequest = firstParser.tryTakeFrame();
        QCOMPARE(firstRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const auto firstDecoded = MiniCloud::Protocol::deserializeAuthenticateRequest(firstRequest.frame.payload);
        QCOMPARE(firstDecoded.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(firstDecoded.data.productKey, invalidKey);
        QCOMPARE(firstDecoded.data.deviceId, deviceId);

        const auto invalidPayload = MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::InvalidKey});

        QCOMPARE(invalidPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto invalidFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            firstRequest.frame.header.requestId,
            TaskId{0},
            invalidPayload.payload);

        QCOMPARE(invalidFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(invalidFrame.encodedFrame), static_cast<qint64>(invalidFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QTRY_COMPARE(errorLabel->text(), QStringLiteral("Invalid product key."));
        QVERIFY(activateButton->isEnabled());
        QVERIFY(controller.isConnected());

        productKeyEdit->setText(correctedKey);
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);
        QTRY_VERIFY(errorLabel->text().isEmpty());
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser secondParser;
        secondParser.appendData(serverSocket->readAll());
        const auto secondRequest = secondParser.tryTakeFrame();
        QCOMPARE(secondRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondRequest.frame.header.messageType, MessageType::AuthenticateRequest);
        QVERIFY(secondRequest.frame.header.requestId != firstRequest.frame.header.requestId);

        const auto secondDecoded = MiniCloud::Protocol::deserializeAuthenticateRequest(secondRequest.frame.payload);
        QCOMPARE(secondDecoded.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(secondDecoded.data.productKey, correctedKey);
        QCOMPARE(secondDecoded.data.deviceId, deviceId);

        const auto validPayload = MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});

        QCOMPARE(validPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto validFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            secondRequest.frame.header.requestId,
            TaskId{0},
            validPayload.payload);

        QCOMPARE(validFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(validFrame.encodedFrame), static_cast<qint64>(validFrame.encodedFrame.size()));
        QVERIFY(serverSocket->flush());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isConnected());
        QVERIFY(controller.isFeatureAccessAllowed());
        QTRY_COMPARE(stack->currentWidget(), filePage);
        QVERIFY(filePage->isEnabled());
        QVERIFY(!activateButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(errorLabel->text().isEmpty());
        QCOMPARE(connectionSpy.count(), 1);
        QCOMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
    }

    void connectButton_closedEndpoint_showsConnectionErrorAndAllowsRetry()
    {
        QTcpServer portReservation;
        QVERIFY(portReservation.listen(QHostAddress::LocalHost, 0));

        const quint16 closedPort = portReservation.serverPort();
        portReservation.close();

        ApplicationController controller;
        MainWindow window(&controller);

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);

        const QString initialStatus = statusLabel->text();
        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(closedPort);
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_COMPARE(errorLabel->text(), QStringLiteral("Unable to connect to server."));
        QVERIFY(!controller.isConnected());
        QCOMPARE(controller.accessState(), MiniCloud::Client::ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QCOMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isEnabled());
        QVERIFY(connectButton->isEnabled());
        QVERIFY(!disconnectButton->isEnabled());
        QVERIFY(!activateButton->isEnabled());
        QVERIFY(hostEdit->isEnabled());
        QVERIFY(portSpinBox->isEnabled());
        QVERIFY(productKeyEdit->isEnabled());
        QVERIFY(deviceIdEdit->isEnabled());
        QVERIFY(!statusLabel->text().trimmed().isEmpty());
        QVERIFY(statusLabel->text() != initialStatus);
    }
};

QTEST_MAIN(MainWindowTest)
#include "mainwindowtest.moc"
