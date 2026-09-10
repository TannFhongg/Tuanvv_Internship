#include <QtTest/QtTest>

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QJsonObject>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "applicationcontroller.h"
#include "authentication.h"
#include "errorresponse.h"
#include "fileprotocol.h"
#include "frameparser.h"
#include "mainwindow.h"
#include "protocolcodec.h"
#include "protocolconstants.h"
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

    void authenticateResponse_valid_requestsRootBrowseAndShowsEntries()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *searchButton = window.findChild<QPushButton *>("searchButton");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(upButton != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(searchButton != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser browseParser;
        browseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult browseRequest = browseParser.tryTakeFrame();
        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const MiniCloud::Protocol::BrowseRequestDecodeResult decodedBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(browseRequest.frame.payload);
        QCOMPARE(decodedBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> entries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("readme.txt"), QStringLiteral("/readme.txt"), FileEntryType::File, 42, 1700000001000}};
        const auto browsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), entries});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto browseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            browseRequest.frame.header.requestId,
            browseRequest.frame.header.taskId,
            browsePayload.payload);
        QCOMPARE(browseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(browseResponse.encodedFrame),
            static_cast<qint64>(browseResponse.encodedFrame.size()));

        QTRY_VERIFY(filePage->isVisible());
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));
        QCOMPARE(fileTable->item(0, 1)->text(), QStringLiteral("Directory"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("readme.txt"));
        QCOMPARE(fileTable->item(1, 1)->text(), QStringLiteral("File"));
        QCOMPARE(fileTable->item(1, 0)->data(Qt::UserRole).toString(), QStringLiteral("/readme.txt"));
        QVERIFY(!upButton->isEnabled());
        QVERIFY(refreshButton->isEnabled());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(searchButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void openDirectoryEntry_requestsNestedBrowseAndShowsChildEntries()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *upButton = window.findChild<QPushButton *>("upButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(upButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(authenticationResponse.encodedFrame), static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRootBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(rootBrowseRequest.frame.payload);
        QCOMPARE(decodedRootBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRootBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};

        const auto rootBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(rootBrowseResponse.encodedFrame), static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents"));
        QVERIFY(!upButton->isEnabled());

        const QRect documentsCell = fileTable->visualItemRect(fileTable->item(0, 0));
        QTest::mouseDClick(fileTable->viewport(), Qt::LeftButton, Qt::NoModifier, documentsCell.center());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser nestedBrowseParser;
        nestedBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult nestedBrowseRequest = nestedBrowseParser.tryTakeFrame();
        QCOMPARE(nestedBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(nestedBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedNestedBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(nestedBrowseRequest.frame.payload);
        QCOMPARE(decodedNestedBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedNestedBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> childEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto childBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), childEntries});
        QCOMPARE(childBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto childBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            nestedBrowseRequest.frame.header.requestId,
            nestedBrowseRequest.frame.header.taskId,
            childBrowsePayload.payload);
        QCOMPARE(childBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(childBrowseResponse.encodedFrame), static_cast<qint64>(childBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QCOMPARE(fileTable->item(0, 1)->text(), QStringLiteral("File"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/report.txt"));
        QVERIFY(upButton->isEnabled());
    }

    void upButton_fromNestedDirectory_requestsParentBrowseAndRestoresRootState()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *upButton = window.findChild<QPushButton *>("upButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(upButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(authenticationResponse.encodedFrame), static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser initialBrowseParser;
        initialBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult initialBrowseRequest = initialBrowseParser.tryTakeFrame();
        QCOMPARE(initialBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(initialBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedInitialBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(initialBrowseRequest.frame.payload);
        QCOMPARE(decodedInitialBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedInitialBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto initialBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(initialBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto initialBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            initialBrowseRequest.frame.header.requestId,
            initialBrowseRequest.frame.header.taskId,
            initialBrowsePayload.payload);
        QCOMPARE(initialBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(initialBrowseResponse.encodedFrame), static_cast<qint64>(initialBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 1);

        const QRect documentsCell = fileTable->visualItemRect(fileTable->item(0, 0));
        QTest::mouseDClick(fileTable->viewport(), Qt::LeftButton, Qt::NoModifier, documentsCell.center());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser nestedBrowseParser;
        nestedBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult nestedBrowseRequest = nestedBrowseParser.tryTakeFrame();
        QCOMPARE(nestedBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(nestedBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedNestedBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(nestedBrowseRequest.frame.payload);
        QCOMPARE(decodedNestedBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedNestedBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> nestedEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto nestedBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), nestedEntries});
        QCOMPARE(nestedBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto nestedBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            nestedBrowseRequest.frame.header.requestId,
            nestedBrowseRequest.frame.header.taskId,
            nestedBrowsePayload.payload);
        QCOMPARE(nestedBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(nestedBrowseResponse.encodedFrame), static_cast<qint64>(nestedBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QVERIFY(upButton->isEnabled());

        QTest::mouseClick(upButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parentBrowseParser;
        parentBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult parentBrowseRequest = parentBrowseParser.tryTakeFrame();
        QCOMPARE(parentBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parentBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedParentBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(parentBrowseRequest.frame.payload);
        QCOMPARE(decodedParentBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedParentBrowseRequest.data.path, QStringLiteral("/"));
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));

        const QList<FileEntryData> restoredRootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("readme.txt"), QStringLiteral("/readme.txt"), FileEntryType::File, 42, 1700000001000}};
        const auto restoredRootPayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), restoredRootEntries});
        QCOMPARE(restoredRootPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto restoredRootResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            parentBrowseRequest.frame.header.requestId,
            parentBrowseRequest.frame.header.taskId,
            restoredRootPayload.payload);
        QCOMPARE(restoredRootResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(restoredRootResponse.encodedFrame), static_cast<qint64>(restoredRootResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));
        QCOMPARE(fileTable->item(0, 1)->text(), QStringLiteral("Directory"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("readme.txt"));
        QCOMPARE(fileTable->item(1, 1)->text(), QStringLiteral("File"));
        QCOMPARE(fileTable->item(1, 0)->data(Qt::UserRole).toString(), QStringLiteral("/readme.txt"));
        QVERIFY(!upButton->isEnabled());
    }

    void refreshButton_requestsCurrentPathAndReplacesEntries()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(refreshButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(authenticationResponse.encodedFrame), static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRootBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(rootBrowseRequest.frame.payload);
        QCOMPARE(decodedRootBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRootBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(rootBrowseResponse.encodedFrame), static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 1);

        const QRect documentsCell = fileTable->visualItemRect(fileTable->item(0, 0));
        QTest::mouseDClick(fileTable->viewport(), Qt::LeftButton, Qt::NoModifier, documentsCell.center());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> initialDocumentsEntries{
            {QStringLiteral("old-report.txt"), QStringLiteral("/Documents/old-report.txt"), FileEntryType::File, 64, 1700000001000},
            {QStringLiteral("Drafts"), QStringLiteral("/Documents/Drafts"), FileEntryType::Directory, 0, 1700000002000}};
        const auto documentsBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), initialDocumentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(documentsBrowseResponse.encodedFrame), static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("old-report.txt"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("Drafts"));

        QTest::mouseClick(refreshButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedDocumentsEntries{
            {QStringLiteral("summary.txt"), QStringLiteral("/Documents/summary.txt"), FileEntryType::File, 128, 1700000003000}};
        const auto refreshBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedDocumentsEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(refreshBrowseResponse.encodedFrame), static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("summary.txt"));
        QCOMPARE(fileTable->item(0, 1)->text(), QStringLiteral("File"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/summary.txt"));
    }

    void searchButton_validQuery_requestsSearchAndShowsResultsWithoutChangingCurrentPath()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::SearchRequestDecodeResult;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *searchEdit = window.findChild<QLineEdit *>("searchEdit");
        auto *searchButton = window.findChild<QPushButton *>("searchButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(searchEdit != nullptr);
        QVERIFY(searchButton != nullptr);
        QVERIFY(upButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(authenticationResponse.encodedFrame), static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRootBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(rootBrowseRequest.frame.payload);
        QCOMPARE(decodedRootBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRootBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(rootBrowseResponse.encodedFrame), static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 1);

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));

        QTRY_VERIFY_WITH_TIMEOUT(serverSocket->bytesAvailable() > 0, 10000);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000},
            {QStringLiteral("image.png"), QStringLiteral("/Documents/image.png"), FileEntryType::File, 256, 1700000002000}};
        const auto documentsBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(documentsBrowseResponse.encodedFrame), static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QVERIFY(upButton->isEnabled());

        searchEdit->setText(QStringLiteral("report"));
        QTest::mouseClick(searchButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser searchParser;
        searchParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult searchRequest = searchParser.tryTakeFrame();
        QCOMPARE(searchRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(searchRequest.frame.header.messageType, MessageType::SearchRequest);

        const SearchRequestDecodeResult decodedSearchRequest =
            MiniCloud::Protocol::deserializeSearchRequest(searchRequest.frame.payload);
        QCOMPARE(decodedSearchRequest.status, SearchRequestDecodeResult::Status::Success);
        QCOMPARE(decodedSearchRequest.data.path, QStringLiteral("/Documents"));
        QCOMPARE(decodedSearchRequest.data.query, QStringLiteral("report"));

        const QList<FileEntryData> searchResults{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000},
            {QStringLiteral("Reports"), QStringLiteral("/Documents/Reports"), FileEntryType::Directory, 0, 1700000003000}};
        const auto searchPayload = MiniCloud::Protocol::serializeSearchResponse({QStringLiteral("/Documents"), searchResults});
        QCOMPARE(searchPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto searchResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::SearchResponse,
            searchRequest.frame.header.requestId,
            searchRequest.frame.header.taskId,
            searchPayload.payload);
        QCOMPARE(searchResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(searchResponse.encodedFrame), static_cast<qint64>(searchResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QTRY_COMPARE(fileTable->item(1, 0)->text(), QStringLiteral("Reports"));
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QCOMPARE(fileTable->item(0, 1)->text(), QStringLiteral("File"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/report.txt"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("Reports"));
        QCOMPARE(fileTable->item(1, 1)->text(), QStringLiteral("Directory"));
        QCOMPARE(fileTable->item(1, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/Reports"));
        QVERIFY(upButton->isEnabled());
    }

    void searchButton_blankQuery_showsValidationErrorWithoutSendingRequest_data()
    {
        QTest::addColumn<QString>("query");

        QTest::newRow("empty") << QString();
        QTest::newRow("whitespace") << QStringLiteral("   ");
    }

    void searchButton_blankQuery_showsValidationErrorWithoutSendingRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(QString, query);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *searchEdit = window.findChild<QLineEdit *>("searchEdit");
        auto *searchButton = window.findChild<QPushButton *>("searchButton");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(searchEdit != nullptr);
        QVERIFY(searchButton != nullptr);
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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser browseParser;
        browseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult browseRequest = browseParser.tryTakeFrame();
        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(browseRequest.frame.payload);
        QCOMPARE(decodedBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> entries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("readme.txt"), QStringLiteral("/readme.txt"), FileEntryType::File, 42, 1700000001000}};
        const auto browsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), entries});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto browseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            browseRequest.frame.header.requestId,
            browseRequest.frame.header.taskId,
            browsePayload.payload);
        QCOMPARE(browseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(browseResponse.encodedFrame),
            static_cast<qint64>(browseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("readme.txt"));

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        searchEdit->setText(query);
        QTest::mouseClick(searchButton, Qt::LeftButton);

        QCOMPARE(errorLabel->text(), QStringLiteral("Search query is required."));
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QCOMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("readme.txt"));

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void createDirectory_validName_sendsRequestThenRefreshesCurrentDirectory()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::CreateDirectoryRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *newDirectoryNameEdit = window.findChild<QLineEdit *>("newDirectoryNameEdit");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(newDirectoryNameEdit != nullptr);
        QVERIFY(createDirectoryButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> initialEntries{
            {QStringLiteral("old.txt"), QStringLiteral("/Documents/old.txt"), FileEntryType::File, 42, 1700000001000}};
        const auto documentsBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), initialEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("old.txt"));

        newDirectoryNameEdit->setText(QStringLiteral("Reports"));
        QTest::mouseClick(createDirectoryButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser createDirectoryParser;
        createDirectoryParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult createDirectoryRequest = createDirectoryParser.tryTakeFrame();
        QCOMPARE(createDirectoryRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(createDirectoryRequest.frame.header.messageType, MessageType::CreateDirectoryRequest);

        const CreateDirectoryRequestDecodeResult decodedCreateDirectoryRequest =
            MiniCloud::Protocol::deserializeCreateDirectoryRequest(createDirectoryRequest.frame.payload);
        QCOMPARE(decodedCreateDirectoryRequest.status, CreateDirectoryRequestDecodeResult::Status::Success);
        QCOMPARE(decodedCreateDirectoryRequest.data.parentPath, QStringLiteral("/Documents"));
        QCOMPARE(decodedCreateDirectoryRequest.data.name, QStringLiteral("Reports"));

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Documents/Reports")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            createDirectoryRequest.frame.header.requestId,
            createDirectoryRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedEntries{
            {QStringLiteral("Reports"), QStringLiteral("/Documents/Reports"), FileEntryType::Directory, 0, 1700000002000},
            {QStringLiteral("old.txt"), QStringLiteral("/Documents/old.txt"), FileEntryType::File, 42, 1700000001000}};
        const auto refreshBrowsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(refreshBrowseResponse.encodedFrame),
            static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QTRY_COMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Reports"));
        QCOMPARE(fileTable->item(0, 1)->text(), QStringLiteral("Directory"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/Reports"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("old.txt"));
        QCOMPARE(newDirectoryNameEdit->text(), QString());
    }

    void createDirectory_blankName_showsValidationErrorWithoutSendingRequest_data()
    {
        QTest::addColumn<QString>("name");

        QTest::newRow("empty") << QString();
        QTest::newRow("whitespace") << QStringLiteral("   ");
    }

    void createDirectory_blankName_showsValidationErrorWithoutSendingRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(QString, name);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *newDirectoryNameEdit = window.findChild<QLineEdit *>("newDirectoryNameEdit");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(newDirectoryNameEdit != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("old.txt"), QStringLiteral("/Documents/old.txt"), FileEntryType::File, 42, 1700000001000},
            {QStringLiteral("Plans"), QStringLiteral("/Documents/Plans"), FileEntryType::Directory, 0, 1700000002000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("old.txt"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("Plans"));

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        newDirectoryNameEdit->setText(name);
        QTest::mouseClick(createDirectoryButton, Qt::LeftButton);

        QCOMPARE(errorLabel->text(), QStringLiteral("Parent path and directory name must not be blank."));
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("old.txt"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("Plans"));

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void renameSelectedEntry_validName_sendsRequestThenRefreshesCurrentDirectory()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::RenameRequestDecodeResult;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *renameNameEdit = window.findChild<QLineEdit *>("renameNameEdit");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(renameNameEdit != nullptr);
        QVERIFY(renameButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 1);

        const QRect documentsCell = fileTable->visualItemRect(fileTable->item(0, 0));
        QTest::mouseDClick(fileTable->viewport(), Qt::LeftButton, Qt::NoModifier, documentsCell.center());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/report.txt"));
        QVERIFY(!renameButton->isEnabled());

        fileTable->selectRow(0);
        QTRY_VERIFY(renameButton->isEnabled());

        renameNameEdit->setText(QStringLiteral("final.txt"));
        QTest::mouseClick(renameButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser renameParser;
        renameParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult renameRequest = renameParser.tryTakeFrame();
        QCOMPARE(renameRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(renameRequest.frame.header.messageType, MessageType::RenameRequest);

        const RenameRequestDecodeResult decodedRenameRequest =
            MiniCloud::Protocol::deserializeRenameRequest(renameRequest.frame.payload);
        QCOMPARE(decodedRenameRequest.status, RenameRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRenameRequest.data.path, QStringLiteral("/Documents/report.txt"));
        QCOMPARE(decodedRenameRequest.data.newName, QStringLiteral("final.txt"));

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Documents/final.txt")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            renameRequest.frame.header.requestId,
            renameRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedEntries{
            {QStringLiteral("final.txt"), QStringLiteral("/Documents/final.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto refreshBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(refreshBrowseResponse.encodedFrame),
            static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QTRY_COMPARE(fileTable->item(0, 0)->text(), QStringLiteral("final.txt"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/final.txt"));
        QCOMPARE(renameNameEdit->text(), QString());
        QVERIFY(!renameButton->isEnabled());
    }

    void renameSelectedEntry_blankName_showsValidationErrorWithoutSendingRequest_data()
    {
        QTest::addColumn<QString>("name");

        QTest::newRow("empty") << QString();
        QTest::newRow("whitespace") << QStringLiteral("   ");
    }

    void renameSelectedEntry_blankName_showsValidationErrorWithoutSendingRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(QString, name);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *renameNameEdit = window.findChild<QLineEdit *>("renameNameEdit");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(renameNameEdit != nullptr);
        QVERIFY(renameButton != nullptr);
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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/report.txt"));

        fileTable->selectRow(0);
        QTRY_VERIFY(renameButton->isEnabled());

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        renameNameEdit->setText(name);
        QTest::mouseClick(renameButton, Qt::LeftButton);

        QCOMPARE(errorLabel->text(), QStringLiteral("New name is required."));
        QCOMPARE(renameNameEdit->text(), name);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QVERIFY(fileTable->item(0, 0)->isSelected());
        QVERIFY(renameButton->isEnabled());

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void moveSelectedEntry_validDestination_sendsRequestThenRefreshesCurrentDirectory()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::MoveRequestDecodeResult;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *moveDestinationPathEdit = window.findChild<QLineEdit *>("moveDestinationPathEdit");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(moveDestinationPathEdit != nullptr);
        QVERIFY(moveButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QVERIFY(!moveButton->isEnabled());

        fileTable->selectRow(0);
        QTRY_VERIFY(moveButton->isEnabled());

        moveDestinationPathEdit->setText(QStringLiteral("/Archive"));
        QTest::mouseClick(moveButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser moveParser;
        moveParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult moveRequest = moveParser.tryTakeFrame();
        QCOMPARE(moveRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(moveRequest.frame.header.messageType, MessageType::MoveRequest);

        const MoveRequestDecodeResult decodedMoveRequest =
            MiniCloud::Protocol::deserializeMoveRequest(moveRequest.frame.payload);
        QCOMPARE(decodedMoveRequest.status, MoveRequestDecodeResult::Status::Success);
        QCOMPARE(decodedMoveRequest.data.sourcePath, QStringLiteral("/Documents/report.txt"));
        QCOMPARE(decodedMoveRequest.data.destinationDirectoryPath, QStringLiteral("/Archive"));

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Archive/report.txt")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            moveRequest.frame.header.requestId,
            moveRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedEntries;
        const auto refreshBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(refreshBrowseResponse.encodedFrame),
            static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 0);
        QCOMPARE(moveDestinationPathEdit->text(), QString());
        QVERIFY(!moveButton->isEnabled());
    }

    void moveSelectedEntry_blankDestination_showsValidationErrorWithoutSendingRequest_data()
    {
        QTest::addColumn<QString>("destination");

        QTest::newRow("empty") << QString();
        QTest::newRow("whitespace") << QStringLiteral("   ");
    }

    void moveSelectedEntry_blankDestination_showsValidationErrorWithoutSendingRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(QString, destination);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *moveDestinationPathEdit = window.findChild<QLineEdit *>("moveDestinationPathEdit");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(moveDestinationPathEdit != nullptr);
        QVERIFY(moveButton != nullptr);
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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));

        fileTable->selectRow(0);
        QTRY_VERIFY(moveButton->isEnabled());

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        moveDestinationPathEdit->setText(destination);
        QTest::mouseClick(moveButton, Qt::LeftButton);

        QCOMPARE(errorLabel->text(), QStringLiteral("Destination path is required."));
        QCOMPARE(moveDestinationPathEdit->text(), destination);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QVERIFY(fileTable->item(0, 0)->isSelected());
        QVERIFY(moveButton->isEnabled());

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void deleteSelectedEntry_cancelledConfirmation_doesNotSendRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));

        fileTable->selectRow(0);
        QVERIFY(fileTable->item(0, 0)->isSelected());

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        bool confirmationShown = false;
        QString confirmationText;
        QTimer::singleShot(
            0,
            &window,
            [&confirmationShown, &confirmationText]()
            {
                auto *confirmationDialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());

                if (confirmationDialog == nullptr)
                {
                    return;
                }

                confirmationShown = true;
                confirmationText = confirmationDialog->text();

                QAbstractButton *noButton = confirmationDialog->button(QMessageBox::No);
                if (noButton != nullptr)
                {
                    noButton->click();
                }
            });

        QTest::mouseClick(deleteButton, Qt::LeftButton);

        QVERIFY(confirmationShown);
        QCOMPARE(confirmationText, QStringLiteral("Delete \"report.txt\"?"));
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QVERIFY(fileTable->item(0, 0)->isSelected());

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void deleteSelectedEntry_confirmed_sendsRequestThenRefreshesCurrentDirectory()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::DeleteRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));

        fileTable->selectRow(0);
        QVERIFY(fileTable->item(0, 0)->isSelected());

        bool confirmationShown = false;
        QString confirmationText;
        QTimer::singleShot(
            0,
            &window,
            [&confirmationShown, &confirmationText]()
            {
                auto *confirmationDialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());

                if (confirmationDialog == nullptr)
                {
                    return;
                }

                confirmationShown = true;
                confirmationText = confirmationDialog->text();

                QAbstractButton *yesButton = confirmationDialog->button(QMessageBox::Yes);
                if (yesButton != nullptr)
                {
                    yesButton->click();
                }
            });

        QTest::mouseClick(deleteButton, Qt::LeftButton);

        QVERIFY(confirmationShown);
        QCOMPARE(confirmationText, QStringLiteral("Delete \"report.txt\"?"));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser deleteParser;
        deleteParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult deleteRequest = deleteParser.tryTakeFrame();
        QCOMPARE(deleteRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(deleteRequest.frame.header.messageType, MessageType::DeleteRequest);

        const DeleteRequestDecodeResult decodedDeleteRequest =
            MiniCloud::Protocol::deserializeDeleteRequest(deleteRequest.frame.payload);
        QCOMPARE(decodedDeleteRequest.status, DeleteRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDeleteRequest.data.path, QStringLiteral("/Documents/report.txt"));

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Documents/report.txt")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            deleteRequest.frame.header.requestId,
            deleteRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedEntries;
        const auto refreshBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(refreshBrowseResponse.encodedFrame),
            static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 0);
        QVERIFY(fileTable->selectedItems().isEmpty());
    }

    void deleteSelectedEntry_errorResponse_showsErrorAndKeepsFileUiUsable()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::DeleteRequestDecodeResult;
        using MiniCloud::Protocol::ErrorCode;
        using MiniCloud::Protocol::ErrorResponseEncodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(errorLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(upButton != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.txt"), QStringLiteral("/Documents/report.txt"), FileEntryType::File, 128, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        fileTable->selectRow(0);
        QTRY_VERIFY(deleteButton->isEnabled());

        bool confirmationShown = false;
        QTimer::singleShot(
            0,
            &window,
            [&confirmationShown]()
            {
                auto *confirmationDialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (confirmationDialog == nullptr)
                {
                    return;
                }

                confirmationShown = true;
                QAbstractButton *yesButton = confirmationDialog->button(QMessageBox::Yes);
                if (yesButton != nullptr)
                {
                    yesButton->click();
                }
            });

        QTest::mouseClick(deleteButton, Qt::LeftButton);
        QVERIFY(confirmationShown);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser deleteParser;
        deleteParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult deleteRequest = deleteParser.tryTakeFrame();
        QCOMPARE(deleteRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(deleteRequest.frame.header.messageType, MessageType::DeleteRequest);

        const DeleteRequestDecodeResult decodedDeleteRequest =
            MiniCloud::Protocol::deserializeDeleteRequest(deleteRequest.frame.payload);
        QCOMPARE(decodedDeleteRequest.status, DeleteRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDeleteRequest.data.path, QStringLiteral("/Documents/report.txt"));

        const QString serverError = QStringLiteral("The selected file no longer exists.");
        const ErrorResponseEncodeResult errorPayload =
            MiniCloud::Protocol::serializeErrorResponse({ErrorCode::FileNotFound, serverError});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);

        const auto errorResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::ErrorResponse,
            deleteRequest.frame.header.requestId,
            deleteRequest.frame.header.taskId,
            errorPayload.payload);
        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(errorResponse.encodedFrame),
            static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_COMPARE(errorLabel->text(), serverError);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(filePage->isEnabled());
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.txt"));
        QVERIFY(fileTable->item(0, 0)->isSelected());
        QVERIFY(refreshButton->isEnabled());
        QVERIFY(upButton->isEnabled());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());
        QVERIFY(downloadButton->isEnabled());
        QVERIFY(renameButton->isEnabled());
        QVERIFY(moveButton->isEnabled());
        QVERIFY(deleteButton->isEnabled());

        QTest::mouseClick(refreshButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser retryBrowseParser;
        retryBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult retryBrowseRequest = retryBrowseParser.tryTakeFrame();
        QCOMPARE(retryBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(retryBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRetryBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(retryBrowseRequest.frame.payload);
        QCOMPARE(decodedRetryBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRetryBrowseRequest.data.path, QStringLiteral("/Documents"));
    }

    void uploadButton_validLocalFile_sendsTransferShowsProgressAndRefreshesCurrentDirectory()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileChunkDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;
        using MiniCloud::Protocol::UploadStartRequestDecodeResult;

        constexpr qsizetype maximumChunkSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes);
        const QByteArray firstChunk(maximumChunkSize, 'a');
        const QByteArray finalChunk(1, 'b');
        const QByteArray localFileContents = firstChunk + finalChunk;

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QFile localFile(localFilePath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        QCOMPARE(localFile.write(localFileContents), static_cast<qint64>(localFileContents.size()));
        localFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *uploadLocalPathEdit = window.findChild<QLineEdit *>("uploadLocalPathEdit");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(uploadLocalPathEdit != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(transferProgressBar != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> initialEntries;
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), initialEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 0);

        uploadLocalPathEdit->setText(localFilePath);
        QTest::mouseClick(uploadButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser uploadStartParser;
        uploadStartParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult uploadStartRequest = uploadStartParser.tryTakeFrame();
        QCOMPARE(uploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);

        const UploadStartRequestDecodeResult decodedUploadStartRequest =
            MiniCloud::Protocol::deserializeUploadStartRequest(uploadStartRequest.frame.payload);
        QCOMPARE(decodedUploadStartRequest.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedUploadStartRequest.data.destinationDirectoryPath, QStringLiteral("/Documents"));
        QCOMPARE(decodedUploadStartRequest.data.fileName, QStringLiteral("report.bin"));
        QCOMPARE(decodedUploadStartRequest.data.totalSizeBytes, static_cast<quint64>(localFileContents.size()));

        const auto readyPayload =
            MiniCloud::Protocol::serializeUploadReadyResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(readyPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto readyResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::UploadReadyResponse,
            uploadStartRequest.frame.header.requestId,
            uploadStartRequest.frame.header.taskId,
            readyPayload.payload);
        QCOMPARE(readyResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(readyResponse.encodedFrame),
            static_cast<qint64>(readyResponse.encodedFrame.size()));

        FrameParser uploadChunkParser;
        QList<MiniCloud::Protocol::ProtocolFrame> uploadChunkFrames;
        const auto collectUploadChunks = [&uploadChunkParser, &uploadChunkFrames, serverSocket]()
        {
            uploadChunkParser.appendData(serverSocket->readAll());

            while (true)
            {
                const FrameParser::FrameParseResult frameResult = uploadChunkParser.tryTakeFrame();
                if (frameResult.status != FrameParser::FrameParseStatus::FrameReady)
                {
                    return;
                }

                uploadChunkFrames.append(frameResult.frame);
            }
        };

        QTRY_VERIFY_WITH_TIMEOUT((collectUploadChunks(), uploadChunkFrames.size() == 2), 1000);
        QTest::qWait(100);
        collectUploadChunks();
        QCOMPARE(uploadChunkFrames.size(), 2);

        const MiniCloud::Protocol::ProtocolFrame &firstChunkFrame = uploadChunkFrames.at(0);
        QCOMPARE(firstChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(firstChunkFrame.header.requestId, uploadStartRequest.frame.header.requestId);
        QCOMPARE(firstChunkFrame.header.taskId, uploadStartRequest.frame.header.taskId);
        const FileChunkDecodeResult decodedFirstChunk =
            MiniCloud::Protocol::deserializeFileChunk(firstChunkFrame.payload);
        QCOMPARE(decodedFirstChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedFirstChunk.data.offset, quint64{0});
        QCOMPARE(decodedFirstChunk.data.bytes, firstChunk);

        const MiniCloud::Protocol::ProtocolFrame &finalChunkFrame = uploadChunkFrames.at(1);
        QCOMPARE(finalChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(finalChunkFrame.header.requestId, uploadStartRequest.frame.header.requestId);
        QCOMPARE(finalChunkFrame.header.taskId, uploadStartRequest.frame.header.taskId);
        const FileChunkDecodeResult decodedFinalChunk =
            MiniCloud::Protocol::deserializeFileChunk(finalChunkFrame.payload);
        QCOMPARE(decodedFinalChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedFinalChunk.data.offset, static_cast<quint64>(firstChunk.size()));
        QCOMPARE(decodedFinalChunk.data.bytes, finalChunk);

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            uploadStartRequest.frame.header.requestId,
            uploadStartRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(transferProgressBar->value(), 100);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedEntries{
            {QStringLiteral("report.bin"), QStringLiteral("/Documents/report.bin"), FileEntryType::File, 65537, 1700000001000}};
        const auto refreshBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(refreshBrowseResponse.encodedFrame),
            static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QTRY_COMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.bin"));
        QCOMPARE(fileTable->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("/Documents/report.bin"));
    }

    void uploadInProgress_disablesConflictingFileActionsAndRestoresThemAfterCompletion()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileChunkDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;
        using MiniCloud::Protocol::UploadStartRequestDecodeResult;

        constexpr qsizetype maximumChunkSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes);
        const QByteArray firstChunk(maximumChunkSize, 'a');
        const QByteArray finalChunk(1, 'b');
        const QByteArray localFileContents = firstChunk + finalChunk;

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("upload.bin"));
        QFile localFile(localFilePath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        QCOMPARE(localFile.write(localFileContents), static_cast<qint64>(localFileContents.size()));
        localFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *disconnectButton = window.findChild<QPushButton *>("disconnectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");
        auto *newDirectoryNameEdit = window.findChild<QLineEdit *>("newDirectoryNameEdit");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *searchEdit = window.findChild<QLineEdit *>("searchEdit");
        auto *searchButton = window.findChild<QPushButton *>("searchButton");
        auto *uploadLocalPathEdit = window.findChild<QLineEdit *>("uploadLocalPathEdit");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadLocalPathEdit = window.findChild<QLineEdit *>("downloadLocalPathEdit");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameNameEdit = window.findChild<QLineEdit *>("renameNameEdit");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveDestinationPathEdit = window.findChild<QLineEdit *>("moveDestinationPathEdit");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(disconnectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(upButton != nullptr);
        QVERIFY(newDirectoryNameEdit != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(searchEdit != nullptr);
        QVERIFY(searchButton != nullptr);
        QVERIFY(uploadLocalPathEdit != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadLocalPathEdit != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameNameEdit != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveDestinationPathEdit != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);
        QVERIFY(transferProgressBar != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> initialEntries{
            {QStringLiteral("report.bin"), QStringLiteral("/Documents/report.bin"), FileEntryType::File, 10, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), initialEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        fileTable->selectRow(0);
        QTRY_VERIFY(downloadButton->isEnabled());
        QTRY_VERIFY(renameButton->isEnabled());
        QTRY_VERIFY(moveButton->isEnabled());
        QTRY_VERIFY(deleteButton->isEnabled());

        uploadLocalPathEdit->setText(localFilePath);
        QTest::mouseClick(uploadButton, Qt::LeftButton);

        QTRY_VERIFY(!filePage->isEnabled());
        QVERIFY(disconnectButton->isEnabled());
        QVERIFY(!refreshButton->isEnabled());
        QVERIFY(!upButton->isEnabled());
        QVERIFY(!newDirectoryNameEdit->isEnabled());
        QVERIFY(!createDirectoryButton->isEnabled());
        QVERIFY(!searchEdit->isEnabled());
        QVERIFY(!searchButton->isEnabled());
        QVERIFY(!uploadLocalPathEdit->isEnabled());
        QVERIFY(!uploadButton->isEnabled());
        QVERIFY(!downloadLocalPathEdit->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!renameNameEdit->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveDestinationPathEdit->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
        QVERIFY(!fileTable->isEnabled());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser uploadStartParser;
        uploadStartParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult uploadStartRequest = uploadStartParser.tryTakeFrame();
        QCOMPARE(uploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);

        const UploadStartRequestDecodeResult decodedUploadStartRequest =
            MiniCloud::Protocol::deserializeUploadStartRequest(uploadStartRequest.frame.payload);
        QCOMPARE(decodedUploadStartRequest.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedUploadStartRequest.data.destinationDirectoryPath, QStringLiteral("/Documents"));
        QCOMPARE(decodedUploadStartRequest.data.fileName, QStringLiteral("upload.bin"));
        QCOMPARE(decodedUploadStartRequest.data.totalSizeBytes, static_cast<quint64>(localFileContents.size()));

        const auto readyPayload =
            MiniCloud::Protocol::serializeUploadReadyResponse({QStringLiteral("/Documents/upload.bin")});
        QCOMPARE(readyPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto readyResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::UploadReadyResponse,
            uploadStartRequest.frame.header.requestId,
            uploadStartRequest.frame.header.taskId,
            readyPayload.payload);
        QCOMPARE(readyResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(readyResponse.encodedFrame),
            static_cast<qint64>(readyResponse.encodedFrame.size()));

        FrameParser uploadChunkParser;
        QList<MiniCloud::Protocol::ProtocolFrame> uploadChunkFrames;
        const auto collectUploadChunks = [&uploadChunkParser, &uploadChunkFrames, serverSocket]()
        {
            uploadChunkParser.appendData(serverSocket->readAll());

            while (true)
            {
                const FrameParser::FrameParseResult frameResult = uploadChunkParser.tryTakeFrame();
                if (frameResult.status != FrameParser::FrameParseStatus::FrameReady)
                {
                    return;
                }

                uploadChunkFrames.append(frameResult.frame);
            }
        };

        QTRY_VERIFY_WITH_TIMEOUT((collectUploadChunks(), uploadChunkFrames.size() == 2), 1000);
        QTest::qWait(100);
        collectUploadChunks();
        QCOMPARE(uploadChunkFrames.size(), 2);

        const MiniCloud::Protocol::ProtocolFrame &firstChunkFrame = uploadChunkFrames.at(0);
        QCOMPARE(firstChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(firstChunkFrame.header.requestId, uploadStartRequest.frame.header.requestId);
        QCOMPARE(firstChunkFrame.header.taskId, uploadStartRequest.frame.header.taskId);
        const FileChunkDecodeResult decodedFirstChunk =
            MiniCloud::Protocol::deserializeFileChunk(firstChunkFrame.payload);
        QCOMPARE(decodedFirstChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedFirstChunk.data.offset, quint64{0});
        QCOMPARE(decodedFirstChunk.data.bytes, firstChunk);

        const MiniCloud::Protocol::ProtocolFrame &finalChunkFrame = uploadChunkFrames.at(1);
        QCOMPARE(finalChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(finalChunkFrame.header.requestId, uploadStartRequest.frame.header.requestId);
        QCOMPARE(finalChunkFrame.header.taskId, uploadStartRequest.frame.header.taskId);
        const FileChunkDecodeResult decodedFinalChunk =
            MiniCloud::Protocol::deserializeFileChunk(finalChunkFrame.payload);
        QCOMPARE(decodedFinalChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedFinalChunk.data.offset, static_cast<quint64>(firstChunk.size()));
        QCOMPARE(decodedFinalChunk.data.bytes, finalChunk);

        QTRY_COMPARE(transferProgressBar->value(), 100);
        QVERIFY(transferProgressBar->isVisible());
        QVERIFY(!refreshButton->isEnabled());
        QVERIFY(!createDirectoryButton->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
        QVERIFY(!uploadButton->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!upButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Documents/upload.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            uploadStartRequest.frame.header.requestId,
            uploadStartRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser refreshBrowseParser;
        refreshBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult refreshBrowseRequest = refreshBrowseParser.tryTakeFrame();
        QCOMPARE(refreshBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(refreshBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRefreshBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(refreshBrowseRequest.frame.payload);
        QCOMPARE(decodedRefreshBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRefreshBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> refreshedEntries{
            {QStringLiteral("report.bin"), QStringLiteral("/Documents/report.bin"), FileEntryType::File, 10, 1700000001000},
            {QStringLiteral("upload.bin"), QStringLiteral("/Documents/upload.bin"), FileEntryType::File, 65537, 1700000002000}};
        const auto refreshBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), refreshedEntries});
        QCOMPARE(refreshBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto refreshBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            refreshBrowseRequest.frame.header.requestId,
            refreshBrowseRequest.frame.header.taskId,
            refreshBrowsePayload.payload);
        QCOMPARE(refreshBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(refreshBrowseResponse.encodedFrame),
            static_cast<qint64>(refreshBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(transferProgressBar->value(), 100);
        QTRY_VERIFY(filePage->isEnabled());
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QVERIFY(fileTable->selectedItems().isEmpty());
        QVERIFY(uploadLocalPathEdit->text().isEmpty());
        QVERIFY(refreshButton->isEnabled());
        QVERIFY(upButton->isEnabled());
        QVERIFY(newDirectoryNameEdit->isEnabled());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());

        fileTable->selectRow(0);
        QTRY_VERIFY(downloadButton->isEnabled());
        QTRY_VERIFY(renameButton->isEnabled());
        QTRY_VERIFY(moveButton->isEnabled());
        QTRY_VERIFY(deleteButton->isEnabled());
    }

    void uploadButton_missingLocalPath_showsValidationErrorWithoutSendingRequest_data()
    {
        QTest::addColumn<QString>("localPath");
        QTest::addColumn<bool>("useMissingPath");
        QTest::addColumn<QString>("expectedError");

        QTest::newRow("empty") << QString() << false << QStringLiteral("Local file path is required.");
        QTest::newRow("whitespace") << QStringLiteral("   ") << false << QStringLiteral("Local file path is required.");
        QTest::newRow("nonexistent") << QString() << true << QStringLiteral("The selected upload file does not exist.");
    }

    void uploadButton_missingLocalPath_showsValidationErrorWithoutSendingRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(QString, localPath);
        QFETCH(bool, useMissingPath);
        QFETCH(QString, expectedError);

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString inputPath = useMissingPath
                                      ? localDirectory.filePath(QStringLiteral("missing-report.bin"))
                                      : localPath;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *uploadLocalPathEdit = window.findChild<QLineEdit *>("uploadLocalPathEdit");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(uploadLocalPathEdit != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(transferProgressBar != nullptr);
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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRootBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(rootBrowseRequest.frame.payload);
        QCOMPARE(decodedRootBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRootBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());
        QSignalSpy uploadProgressSpy(&controller, &ApplicationController::uploadProgress);
        QVERIFY(uploadProgressSpy.isValid());

        uploadLocalPathEdit->setText(inputPath);
        QTest::mouseClick(uploadButton, Qt::LeftButton);

        QCOMPARE(errorLabel->text(), expectedError);
        QCOMPARE(transferProgressBar->value(), 0);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(uploadProgressSpy.count(), 0);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void downloadSelectedFile_validLocalTarget_receivesTransferShowsProgressWithoutRefreshingRemoteList()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::DownloadRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        constexpr qsizetype maximumChunkSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes);
        const QByteArray firstChunk(maximumChunkSize, 'a');
        const QByteArray finalChunk(1, 'b');
        const QByteArray expectedContents = firstChunk + finalChunk;

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localTargetPath = localDirectory.filePath(QStringLiteral("report.bin"));
        QVERIFY(!QFileInfo::exists(localTargetPath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *downloadLocalPathEdit = window.findChild<QLineEdit *>("downloadLocalPathEdit");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(downloadLocalPathEdit != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(transferProgressBar != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.bin"), QStringLiteral("/Documents/report.bin"), FileEntryType::File, 65537, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.bin"));
        QVERIFY(!downloadButton->isEnabled());

        fileTable->selectRow(0);
        QTRY_VERIFY(downloadButton->isEnabled());

        downloadLocalPathEdit->setText(localTargetPath);
        QTest::mouseClick(downloadButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadParser;
        downloadParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);

        const DownloadRequestDecodeResult decodedDownloadRequest =
            MiniCloud::Protocol::deserializeDownloadRequest(downloadRequest.frame.payload);
        QCOMPARE(decodedDownloadRequest.status, DownloadRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDownloadRequest.data.path, QStringLiteral("/Documents/report.bin"));

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        const auto startPayload = MiniCloud::Protocol::serializeDownloadStartResponse(
            {QStringLiteral("/Documents/report.bin"), static_cast<quint64>(expectedContents.size())});
        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto startResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequest.frame.header.requestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);
        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        const auto firstChunkPayload = MiniCloud::Protocol::serializeFileChunk({quint64{0}, firstChunk});
        QCOMPARE(firstChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto firstChunkFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::FileChunk,
            downloadRequest.frame.header.requestId,
            downloadRequest.frame.header.taskId,
            firstChunkPayload.payload);
        QCOMPARE(firstChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(firstChunkFrame.encodedFrame),
            static_cast<qint64>(firstChunkFrame.encodedFrame.size()));

        const auto finalChunkPayload = MiniCloud::Protocol::serializeFileChunk(
            {static_cast<quint64>(firstChunk.size()), finalChunk});
        QCOMPARE(finalChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto finalChunkFrame = MiniCloud::Protocol::serializeFrame(
            MessageType::FileChunk,
            downloadRequest.frame.header.requestId,
            downloadRequest.frame.header.taskId,
            finalChunkPayload.payload);
        QCOMPARE(finalChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(finalChunkFrame.encodedFrame),
            static_cast<qint64>(finalChunkFrame.encodedFrame.size()));

        const auto completionPayload =
            MiniCloud::Protocol::serializeFileOperationResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::FileOperationResponse,
            downloadRequest.frame.header.requestId,
            downloadRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(transferProgressBar->value(), 100);
        QTRY_VERIFY(QFileInfo::exists(localTargetPath));
        QFile downloadedFile(localTargetPath);
        QVERIFY(downloadedFile.open(QIODevice::ReadOnly));
        QCOMPARE(downloadedFile.readAll(), expectedContents);

        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.bin"));
        QVERIFY(fileTable->item(0, 0)->isSelected());

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
    }

    void downloadSelectedFile_invalidLocalTarget_showsValidationErrorWithoutSendingRequest_data()
    {
        QTest::addColumn<QString>("localTarget");
        QTest::addColumn<bool>("useExistingTarget");
        QTest::addColumn<QString>("expectedError");

        QTest::newRow("empty") << QString() << false << QStringLiteral("Local download target path is required.");
        QTest::newRow("whitespace") << QStringLiteral("   ") << false << QStringLiteral("Local download target path is required.");
        QTest::newRow("existing") << QString() << true << QStringLiteral("The download target path is unavailable.");
    }

    void downloadSelectedFile_invalidLocalTarget_showsValidationErrorWithoutSendingRequest()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QFETCH(QString, localTarget);
        QFETCH(bool, useExistingTarget);
        QFETCH(QString, expectedError);

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString inputTarget = useExistingTarget
                                        ? localDirectory.filePath(QStringLiteral("existing-report.bin"))
                                        : localTarget;

        if (useExistingTarget)
        {
            QFile existingTarget(inputTarget);
            QVERIFY(existingTarget.open(QIODevice::WriteOnly));
            QCOMPARE(existingTarget.write(QByteArrayLiteral("existing")), qint64{8});
        }

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *downloadLocalPathEdit = window.findChild<QLineEdit *>("downloadLocalPathEdit");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(downloadLocalPathEdit != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(transferProgressBar != nullptr);
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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));

        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.bin"), QStringLiteral("/Documents/report.bin"), FileEntryType::File, 65537, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.bin"));

        fileTable->selectRow(0);
        QTRY_VERIFY(downloadButton->isEnabled());

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());
        QSignalSpy downloadProgressSpy(&controller, &ApplicationController::downloadProgress);
        QVERIFY(downloadProgressSpy.isValid());

        downloadLocalPathEdit->setText(inputTarget);
        QTest::mouseClick(downloadButton, Qt::LeftButton);

        QCOMPARE(errorLabel->text(), expectedError);
        QCOMPARE(transferProgressBar->value(), 0);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QCOMPARE(fileTable->rowCount(), 1);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("report.bin"));
        QVERIFY(fileTable->item(0, 0)->isSelected());

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(downloadProgressSpy.count(), 0);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void fileActionButtons_followSelectionAndEntryType()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser browseParser;
        browseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult browseRequest = browseParser.tryTakeFrame();
        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(browseRequest.frame.payload);
        QCOMPARE(decodedBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> entries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("report.bin"), QStringLiteral("/report.bin"), FileEntryType::File, 65537, 1700000001000}};
        const auto browsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), entries});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto browseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            browseRequest.frame.header.requestId,
            browseRequest.frame.header.taskId,
            browsePayload.payload);
        QCOMPARE(browseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(browseResponse.encodedFrame),
            static_cast<qint64>(browseResponse.encodedFrame.size()));

        QTRY_COMPARE(fileTable->rowCount(), 2);
        QVERIFY(fileTable->selectedItems().isEmpty());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
        QVERIFY(!downloadButton->isEnabled());

        fileTable->selectRow(0);
        QTRY_VERIFY(renameButton->isEnabled());
        QTRY_VERIFY(moveButton->isEnabled());
        QTRY_VERIFY(deleteButton->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());

        fileTable->clearSelection();
        fileTable->selectRow(1);
        QTRY_VERIFY(renameButton->isEnabled());
        QTRY_VERIFY(moveButton->isEnabled());
        QTRY_VERIFY(deleteButton->isEnabled());
        QTRY_VERIFY(downloadButton->isEnabled());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());
    }

    void browseError_showsErrorKeepsFilePageAndAllowsRefreshRetry()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::ErrorCode;
        using MiniCloud::Protocol::ErrorResponseEncodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *statusLabel = window.findChild<QLabel *>("statusLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(errorLabel != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(fileTable != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser browseParser;
        browseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = browseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRootBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(rootBrowseRequest.frame.payload);
        QCOMPARE(decodedRootBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRootBrowseRequest.data.path, QStringLiteral("/"));

        const QString browseError = QStringLiteral("The server could not load the root directory.");
        const ErrorResponseEncodeResult errorPayload =
            MiniCloud::Protocol::serializeErrorResponse({ErrorCode::InternalServerError, browseError});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);

        const auto errorResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::ErrorResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            errorPayload.payload);
        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(errorResponse.encodedFrame),
            static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_VERIFY(filePage->isVisible());
        QTRY_COMPARE(errorLabel->text(), browseError);
        QCOMPARE(statusLabel->text(), QStringLiteral("Activated"));
        QVERIFY(refreshButton->isEnabled());
        QCOMPARE(controller.accessState(), ClientAccessState::Active);

        QTest::mouseClick(refreshButton, Qt::LeftButton);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser retryBrowseParser;
        retryBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult retryBrowseRequest = retryBrowseParser.tryTakeFrame();
        QCOMPARE(retryBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(retryBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRetryBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(retryBrowseRequest.frame.payload);
        QCOMPARE(decodedRetryBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRetryBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> retryEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("report.bin"), QStringLiteral("/report.bin"), FileEntryType::File, 65537, 1700000001000}};
        const auto retryBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), retryEntries});
        QCOMPARE(retryBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto retryBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            retryBrowseRequest.frame.header.requestId,
            retryBrowseRequest.frame.header.taskId,
            retryBrowsePayload.payload);
        QCOMPARE(retryBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(retryBrowseResponse.encodedFrame),
            static_cast<qint64>(retryBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Documents"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("report.bin"));
        QTRY_COMPARE(errorLabel->text(), QString());
        QVERIFY(filePage->isVisible());
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
    }

    void disconnectButton_fromFilePage_clearsEntriesAndRestoresLockedActivationUi()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

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
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");
        auto *newDirectoryNameEdit = window.findChild<QLineEdit *>("newDirectoryNameEdit");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *searchEdit = window.findChild<QLineEdit *>("searchEdit");
        auto *searchButton = window.findChild<QPushButton *>("searchButton");
        auto *uploadLocalPathEdit = window.findChild<QLineEdit *>("uploadLocalPathEdit");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadLocalPathEdit = window.findChild<QLineEdit *>("downloadLocalPathEdit");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameNameEdit = window.findChild<QLineEdit *>("renameNameEdit");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveDestinationPathEdit = window.findChild<QLineEdit *>("moveDestinationPathEdit");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

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
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(errorLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(transferProgressBar != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(upButton != nullptr);
        QVERIFY(newDirectoryNameEdit != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(searchEdit != nullptr);
        QVERIFY(searchButton != nullptr);
        QVERIFY(uploadLocalPathEdit != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadLocalPathEdit != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameNameEdit != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveDestinationPathEdit != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser rootBrowseParser;
        rootBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult rootBrowseRequest = rootBrowseParser.tryTakeFrame();
        QCOMPARE(rootBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(rootBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const QList<FileEntryData> rootEntries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000}};
        const auto rootBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), rootEntries});
        QCOMPARE(rootBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto rootBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            rootBrowseRequest.frame.header.requestId,
            rootBrowseRequest.frame.header.taskId,
            rootBrowsePayload.payload);
        QCOMPARE(rootBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(rootBrowseResponse.encodedFrame),
            static_cast<qint64>(rootBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.requestBrowse(QStringLiteral("/Documents")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser documentsBrowseParser;
        documentsBrowseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult documentsBrowseRequest = documentsBrowseParser.tryTakeFrame();
        QCOMPARE(documentsBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(documentsBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedDocumentsBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(documentsBrowseRequest.frame.payload);
        QCOMPARE(decodedDocumentsBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDocumentsBrowseRequest.data.path, QStringLiteral("/Documents"));

        const QList<FileEntryData> documentsEntries{
            {QStringLiteral("report.bin"), QStringLiteral("/Documents/report.bin"), FileEntryType::File, 65537, 1700000000000},
            {QStringLiteral("Reports"), QStringLiteral("/Documents/Reports"), FileEntryType::Directory, 0, 1700000001000}};
        const auto documentsBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/Documents"), documentsEntries});
        QCOMPARE(documentsBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto documentsBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            documentsBrowseRequest.frame.header.requestId,
            documentsBrowseRequest.frame.header.taskId,
            documentsBrowsePayload.payload);
        QCOMPARE(documentsBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(documentsBrowseResponse.encodedFrame),
            static_cast<qint64>(documentsBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(stack->currentWidget(), filePage);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/Documents"));
        QTRY_COMPARE(fileTable->rowCount(), 2);

        errorLabel->setText(QStringLiteral("Stale session error"));
        transferProgressBar->setValue(55);
        fileTable->selectRow(0);
        QTRY_VERIFY(downloadButton->isEnabled());

        QTest::mouseClick(disconnectButton, Qt::LeftButton);

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isVisible());
        QVERIFY(!filePage->isEnabled());
        QCOMPARE(fileTable->rowCount(), 0);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QCOMPARE(errorLabel->text(), QString());
        QCOMPARE(transferProgressBar->value(), 0);
        QVERIFY(fileTable->selectedItems().isEmpty());

        QVERIFY(!refreshButton->isEnabled());
        QVERIFY(!upButton->isEnabled());
        QVERIFY(!newDirectoryNameEdit->isEnabled());
        QVERIFY(!createDirectoryButton->isEnabled());
        QVERIFY(!searchEdit->isEnabled());
        QVERIFY(!searchButton->isEnabled());
        QVERIFY(!uploadLocalPathEdit->isEnabled());
        QVERIFY(!uploadButton->isEnabled());
        QVERIFY(!downloadLocalPathEdit->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!renameNameEdit->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveDestinationPathEdit->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
    }

    void serverDisconnect_fromFilePage_showsConnectionLostAndRestoresLockedActivationUi()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

        auto *hostEdit = window.findChild<QLineEdit *>("hostLineEdit");
        auto *portSpinBox = window.findChild<QSpinBox *>("portSpinBox");
        auto *productKeyEdit = window.findChild<QLineEdit *>("productKeyLineEdit");
        auto *deviceIdEdit = window.findChild<QLineEdit *>("deviceIdLineEdit");
        auto *connectButton = window.findChild<QPushButton *>("connectButton");
        auto *activateButton = window.findChild<QPushButton *>("activateButton");
        auto *stack = window.findChild<QStackedWidget *>("mainStackedWidget");
        auto *activationPage = window.findChild<QWidget *>("activationPage");
        auto *filePage = window.findChild<QWidget *>("filePage");
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *transferProgressBar = window.findChild<QProgressBar *>("transferProgressBar");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");
        auto *newDirectoryNameEdit = window.findChild<QLineEdit *>("newDirectoryNameEdit");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *searchEdit = window.findChild<QLineEdit *>("searchEdit");
        auto *searchButton = window.findChild<QPushButton *>("searchButton");
        auto *uploadLocalPathEdit = window.findChild<QLineEdit *>("uploadLocalPathEdit");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadLocalPathEdit = window.findChild<QLineEdit *>("downloadLocalPathEdit");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameNameEdit = window.findChild<QLineEdit *>("renameNameEdit");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveDestinationPathEdit = window.findChild<QLineEdit *>("moveDestinationPathEdit");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

        QVERIFY(hostEdit != nullptr);
        QVERIFY(portSpinBox != nullptr);
        QVERIFY(productKeyEdit != nullptr);
        QVERIFY(deviceIdEdit != nullptr);
        QVERIFY(connectButton != nullptr);
        QVERIFY(activateButton != nullptr);
        QVERIFY(stack != nullptr);
        QVERIFY(activationPage != nullptr);
        QVERIFY(filePage != nullptr);
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(errorLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(transferProgressBar != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(upButton != nullptr);
        QVERIFY(newDirectoryNameEdit != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(searchEdit != nullptr);
        QVERIFY(searchButton != nullptr);
        QVERIFY(uploadLocalPathEdit != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadLocalPathEdit != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameNameEdit != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveDestinationPathEdit != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);

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
        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            authenticationRequest.frame.header.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser browseParser;
        browseParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult browseRequest = browseParser.tryTakeFrame();
        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedBrowseRequest =
            MiniCloud::Protocol::deserializeBrowseRequest(browseRequest.frame.payload);
        QCOMPARE(decodedBrowseRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedBrowseRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> entries{
            {QStringLiteral("Documents"), QStringLiteral("/Documents"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("report.bin"), QStringLiteral("/report.bin"), FileEntryType::File, 65537, 1700000001000}};
        const auto browsePayload = MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), entries});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto browseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            browseRequest.frame.header.requestId,
            browseRequest.frame.header.taskId,
            browsePayload.payload);
        QCOMPARE(browseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(browseResponse.encodedFrame),
            static_cast<qint64>(browseResponse.encodedFrame.size()));

        QTRY_COMPARE(stack->currentWidget(), filePage);
        QTRY_COMPARE(fileTable->rowCount(), 2);

        errorLabel->setText(QStringLiteral("Stale session error"));
        transferProgressBar->setValue(55);
        QSignalSpy connectionStateSpy(&controller, &ApplicationController::connectionStateChanged);
        QVERIFY(connectionStateSpy.isValid());

        serverSocket->disconnectFromHost();

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QVERIFY(!filePage->isVisible());
        QVERIFY(!filePage->isEnabled());
        QCOMPARE(fileTable->rowCount(), 0);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QCOMPARE(transferProgressBar->value(), 0);
        QCOMPARE(errorLabel->text(), QStringLiteral("Connection lost."));
        QVERIFY(fileTable->selectedItems().isEmpty());

        QVERIFY(!refreshButton->isEnabled());
        QVERIFY(!upButton->isEnabled());
        QVERIFY(!newDirectoryNameEdit->isEnabled());
        QVERIFY(!createDirectoryButton->isEnabled());
        QVERIFY(!searchEdit->isEnabled());
        QVERIFY(!searchButton->isEnabled());
        QVERIFY(!uploadLocalPathEdit->isEnabled());
        QVERIFY(!uploadButton->isEnabled());
        QVERIFY(!downloadLocalPathEdit->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!renameNameEdit->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveDestinationPathEdit->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());

        int disconnectNotificationCount = 0;
        for (const QList<QVariant> &arguments : connectionStateSpy)
        {
            if (!arguments.at(0).toBool())
            {
                ++disconnectNotificationCount;
            }
        }
        QCOMPARE(disconnectNotificationCount, 1);
    }

    void reconnectAfterFilePageDisconnect_reauthenticatesBrowsesRootAndShowsFreshEntries()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Protocol::AuthenticateRequestDecodeResult;
        using MiniCloud::Protocol::AuthenticationEncodeResult;
        using MiniCloud::Protocol::AuthenticationStatus;
        using MiniCloud::Protocol::BrowseRequestDecodeResult;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Protocol::FrameEncodeStatus;
        using MiniCloud::Protocol::FrameParser;
        using MiniCloud::Protocol::MessageType;
        using MiniCloud::Protocol::TaskId;

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        MainWindow window(&controller);
        window.show();

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
        auto *currentPathLabel = window.findChild<QLabel *>("currentPathLabel");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");
        auto *fileTable = window.findChild<QTableWidget *>("fileTable");
        auto *refreshButton = window.findChild<QPushButton *>("refreshButton");
        auto *upButton = window.findChild<QPushButton *>("upButton");
        auto *createDirectoryButton = window.findChild<QPushButton *>("createDirectoryButton");
        auto *uploadButton = window.findChild<QPushButton *>("uploadButton");
        auto *downloadButton = window.findChild<QPushButton *>("downloadButton");
        auto *renameButton = window.findChild<QPushButton *>("renameButton");
        auto *moveButton = window.findChild<QPushButton *>("moveButton");
        auto *deleteButton = window.findChild<QPushButton *>("deleteButton");

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
        QVERIFY(currentPathLabel != nullptr);
        QVERIFY(errorLabel != nullptr);
        QVERIFY(fileTable != nullptr);
        QVERIFY(refreshButton != nullptr);
        QVERIFY(upButton != nullptr);
        QVERIFY(createDirectoryButton != nullptr);
        QVERIFY(uploadButton != nullptr);
        QVERIFY(downloadButton != nullptr);
        QVERIFY(renameButton != nullptr);
        QVERIFY(moveButton != nullptr);
        QVERIFY(deleteButton != nullptr);

        hostEdit->setText(QStringLiteral("127.0.0.1"));
        portSpinBox->setValue(server.serverPort());
        productKeyEdit->setText(productKey);
        deviceIdEdit->setText(deviceId);
        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *firstServerSocket = server.nextPendingConnection();
        QVERIFY(firstServerSocket != nullptr);

        QTRY_VERIFY(activateButton->isEnabled());
        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(firstServerSocket->bytesAvailable() > 0);
        FrameParser firstAuthenticationParser;
        firstAuthenticationParser.appendData(firstServerSocket->readAll());
        const FrameParser::FrameParseResult firstAuthenticationRequest =
            firstAuthenticationParser.tryTakeFrame();
        QCOMPARE(firstAuthenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstAuthenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const auto firstDecodedAuthentication =
            MiniCloud::Protocol::deserializeAuthenticateRequest(firstAuthenticationRequest.frame.payload);
        QCOMPARE(firstDecodedAuthentication.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(firstDecodedAuthentication.data.productKey, productKey);
        QCOMPARE(firstDecodedAuthentication.data.deviceId, deviceId);

        const AuthenticationEncodeResult validPayload =
            MiniCloud::Protocol::serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(validPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto firstAuthenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            firstAuthenticationRequest.frame.header.requestId,
            TaskId{0},
            validPayload.payload);
        QCOMPARE(firstAuthenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            firstServerSocket->write(firstAuthenticationResponse.encodedFrame),
            static_cast<qint64>(firstAuthenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(firstServerSocket->bytesAvailable() > 0);
        FrameParser firstBrowseParser;
        firstBrowseParser.appendData(firstServerSocket->readAll());
        const FrameParser::FrameParseResult firstBrowseRequest = firstBrowseParser.tryTakeFrame();
        QCOMPARE(firstBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult firstDecodedBrowse =
            MiniCloud::Protocol::deserializeBrowseRequest(firstBrowseRequest.frame.payload);
        QCOMPARE(firstDecodedBrowse.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(firstDecodedBrowse.data.path, QStringLiteral("/"));

        const QList<FileEntryData> oldEntries{
            {QStringLiteral("old-folder"), QStringLiteral("/old-folder"), FileEntryType::Directory, 0, 1700000000000},
            {QStringLiteral("old-report.bin"), QStringLiteral("/old-report.bin"), FileEntryType::File, 1024, 1700000001000}};
        const auto firstBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), oldEntries});
        QCOMPARE(firstBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto firstBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            firstBrowseRequest.frame.header.requestId,
            firstBrowseRequest.frame.header.taskId,
            firstBrowsePayload.payload);
        QCOMPARE(firstBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            firstServerSocket->write(firstBrowseResponse.encodedFrame),
            static_cast<qint64>(firstBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(stack->currentWidget(), filePage);
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("old-folder"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("old-report.bin"));

        firstServerSocket->disconnectFromHost();

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QTRY_COMPARE(stack->currentWidget(), activationPage);
        QCOMPARE(fileTable->rowCount(), 0);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(errorLabel->text(), QStringLiteral("Connection lost."));
        QTRY_VERIFY(connectButton->isEnabled());
        QTRY_VERIFY(!activateButton->isEnabled());

        QTest::mouseClick(connectButton, Qt::LeftButton);

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *secondServerSocket = server.nextPendingConnection();
        QVERIFY(secondServerSocket != nullptr);
        QVERIFY(secondServerSocket != firstServerSocket);
        QTRY_VERIFY(activateButton->isEnabled());
        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QCOMPARE(stack->currentWidget(), activationPage);
        QCOMPARE(fileTable->rowCount(), 0);
        QCOMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QCOMPARE(errorLabel->text(), QString());

        QTest::mouseClick(activateButton, Qt::LeftButton);
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(secondServerSocket->bytesAvailable() > 0);
        FrameParser secondAuthenticationParser;
        secondAuthenticationParser.appendData(secondServerSocket->readAll());
        const FrameParser::FrameParseResult secondAuthenticationRequest =
            secondAuthenticationParser.tryTakeFrame();
        QCOMPARE(secondAuthenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondAuthenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);
        QVERIFY(secondAuthenticationRequest.frame.header.requestId !=
                firstAuthenticationRequest.frame.header.requestId);

        const auto secondDecodedAuthentication =
            MiniCloud::Protocol::deserializeAuthenticateRequest(secondAuthenticationRequest.frame.payload);
        QCOMPARE(secondDecodedAuthentication.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(secondDecodedAuthentication.data.productKey, productKey);
        QCOMPARE(secondDecodedAuthentication.data.deviceId, deviceId);

        const auto secondAuthenticationResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateResponse,
            secondAuthenticationRequest.frame.header.requestId,
            TaskId{0},
            validPayload.payload);
        QCOMPARE(secondAuthenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            secondServerSocket->write(secondAuthenticationResponse.encodedFrame),
            static_cast<qint64>(secondAuthenticationResponse.encodedFrame.size()));

        QTRY_VERIFY(secondServerSocket->bytesAvailable() > 0);
        FrameParser secondBrowseParser;
        secondBrowseParser.appendData(secondServerSocket->readAll());
        const FrameParser::FrameParseResult secondBrowseRequest = secondBrowseParser.tryTakeFrame();
        QCOMPARE(secondBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult secondDecodedBrowse =
            MiniCloud::Protocol::deserializeBrowseRequest(secondBrowseRequest.frame.payload);
        QCOMPARE(secondDecodedBrowse.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(secondDecodedBrowse.data.path, QStringLiteral("/"));

        const QList<FileEntryData> freshEntries{
            {QStringLiteral("Archive"), QStringLiteral("/Archive"), FileEntryType::Directory, 0, 1700000002000},
            {QStringLiteral("fresh-report.bin"), QStringLiteral("/fresh-report.bin"), FileEntryType::File, 2048, 1700000003000}};
        const auto secondBrowsePayload =
            MiniCloud::Protocol::serializeBrowseResponse({QStringLiteral("/"), freshEntries});
        QCOMPARE(secondBrowsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto secondBrowseResponse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseResponse,
            secondBrowseRequest.frame.header.requestId,
            secondBrowseRequest.frame.header.taskId,
            secondBrowsePayload.payload);
        QCOMPARE(secondBrowseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            secondServerSocket->write(secondBrowseResponse.encodedFrame),
            static_cast<qint64>(secondBrowseResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QTRY_COMPARE(stack->currentWidget(), filePage);
        QTRY_VERIFY(filePage->isEnabled());
        QTRY_COMPARE(currentPathLabel->text(), QStringLiteral("/"));
        QTRY_COMPARE(fileTable->rowCount(), 2);
        QCOMPARE(fileTable->item(0, 0)->text(), QStringLiteral("Archive"));
        QCOMPARE(fileTable->item(1, 0)->text(), QStringLiteral("fresh-report.bin"));
        QVERIFY(fileTable->item(0, 0)->text() != QStringLiteral("old-folder"));
        QVERIFY(fileTable->item(1, 0)->text() != QStringLiteral("old-report.bin"));
        QVERIFY(fileTable->selectedItems().isEmpty());
        QCOMPARE(errorLabel->text(), QString());

        QVERIFY(refreshButton->isEnabled());
        QVERIFY(!upButton->isEnabled());
        QVERIFY(createDirectoryButton->isEnabled());
        QVERIFY(uploadButton->isEnabled());
        QVERIFY(!downloadButton->isEnabled());
        QVERIFY(!renameButton->isEnabled());
        QVERIFY(!moveButton->isEnabled());
        QVERIFY(!deleteButton->isEnabled());
        QVERIFY(disconnectButton->isEnabled());

        fileTable->selectRow(1);
        QTRY_VERIFY(downloadButton->isEnabled());
        QTRY_VERIFY(renameButton->isEnabled());
        QTRY_VERIFY(moveButton->isEnabled());
        QTRY_VERIFY(deleteButton->isEnabled());
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
