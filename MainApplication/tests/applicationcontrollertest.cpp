#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "applicationcontroller.h"
#include "authentication.h"
#include "errorresponse.h"
#include "frameparser.h"
#include "protocolcodec.h"
#include "protocoltypes.h"
#include "requesttypes.h"

using MiniCloud::Client::ClientAccessState;
using MiniCloud::Client::RequestDispatchError;
using MiniCloud::Client::RequestSendResult;
using MiniCloud::Client::RequestSendStatus;
using MiniCloud::Protocol::AuthenticateRequestDecodeResult;
using MiniCloud::Protocol::AuthenticationEncodeResult;
using MiniCloud::Protocol::AuthenticationStatus;
using MiniCloud::Protocol::deserializeAuthenticateRequest;
using MiniCloud::Protocol::ErrorCode;
using MiniCloud::Protocol::ErrorResponseData;
using MiniCloud::Protocol::ErrorResponseEncodeResult;
using MiniCloud::Protocol::FrameEncodeStatus;
using MiniCloud::Protocol::FrameParser;
using MiniCloud::Protocol::MessageType;
using MiniCloud::Protocol::RequestId;
using MiniCloud::Protocol::serializeAuthenticateResponse;
using MiniCloud::Protocol::serializeErrorResponse;
using MiniCloud::Protocol::serializeFrame;
using MiniCloud::Protocol::TaskId;
class ApplicationControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ClientAccessState>();
        qRegisterMetaType<AuthenticationStatus>();
        qRegisterMetaType<ErrorResponseData>();
        qRegisterMetaType<RequestDispatchError>();
    }

    void initialState_isLockedAndFeatureAccessDenied()
    {
        ApplicationController controller;

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
    }

    void activate_whenDisconnected_returnsNotConnectedAndRemainsLocked()
    {
        ApplicationController controller;
        QCOMPARE(controller.accessState(), ClientAccessState::Locked);

        const RequestSendResult result = controller.activate(QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));

        QCOMPARE(result.status, RequestSendStatus::Failed);

        QCOMPARE(result.requestId, RequestId{0});

        QCOMPARE(result.errorCode, RequestDispatchError::NotConnected);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);

        QVERIFY(!controller.isFeatureAccessAllowed());
    }

    void activate_whenConnected_sendsAuthenticateRequestAndEntersAuthenticating()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();

        QVERIFY(serverSocket != nullptr);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);

        QVERIFY(!controller.isFeatureAccessAllowed());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");

        const QString deviceId = QStringLiteral("DEVICE-CLIENT");

        const RequestSendResult result = controller.activate(productKey, deviceId);

        QCOMPARE(result.status, RequestSendStatus::Accepted);

        QVERIFY(result.requestId != RequestId{0});
        QCOMPARE(result.errorCode, RequestDispatchError::None);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const auto parsed = parser.tryTakeFrame();

        QCOMPARE(parsed.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsed.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(parsed.frame.header.requestId, result.requestId);
        QCOMPARE(parsed.frame.header.taskId, TaskId{0});

        const auto decoded = deserializeAuthenticateRequest(parsed.frame.payload);
        QCOMPARE(decoded.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(decoded.data.productKey, productKey);
        QCOMPARE(decoded.data.deviceId, deviceId);
    }

    void authenticateResponse_valid_transitionsToActiveAndAllowsFeatureAccess()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QSignalSpy stateSpy(&controller, &ApplicationController::accessStateChanged);
        QVERIFY(stateSpy.isValid());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const RequestSendResult requestResult = controller.activate(productKey, deviceId);

        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult request = requestParser.tryTakeFrame();
        QCOMPARE(request.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(request.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(request.frame.header.requestId, requestResult.requestId);

        const auto responsePayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});

        QCOMPARE(responsePayload.status, AuthenticationEncodeResult::Status::Success);

        const auto encodedFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            requestResult.requestId,
            TaskId{0},
            responsePayload.payload);

        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);

        QCOMPARE(serverSocket->write(encodedFrame.encodedFrame), static_cast<qint64>(encodedFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        QCOMPARE(stateSpy.count(), 2);

        QCOMPARE(qvariant_cast<ClientAccessState>(stateSpy.at(0).at(0)), ClientAccessState::Authenticating);
        QCOMPARE(qvariant_cast<ClientAccessState>(stateSpy.at(1).at(0)), ClientAccessState::Active);
    }

    void authenticateResponse_nonValidStatus_returnsLockedAndDeniesFeatureAccess_data()
    {
        QTest::addColumn<AuthenticationStatus>("authenticationStatus");

        QTest::newRow("invalid-key") << AuthenticationStatus::InvalidKey;

        QTest::newRow("disabled") << AuthenticationStatus::Disabled;

        QTest::newRow("device-mismatch") << AuthenticationStatus::DeviceMismatch;
    }
    void authenticateResponse_nonValidStatus_returnsLockedAndDeniesFeatureAccess()
    {
        QFETCH(AuthenticationStatus, authenticationStatus);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QSignalSpy stateSpy(&controller, &ApplicationController::accessStateChanged);
        QVERIFY(stateSpy.isValid());
        QSignalSpy rejectedSpy(&controller, &ApplicationController::activationRejected);
        QVERIFY(rejectedSpy.isValid());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const RequestSendResult requestResult = controller.activate(productKey, deviceId);
        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        const auto responsePayload = serializeAuthenticateResponse({authenticationStatus});

        const auto responseFrame =
            serializeFrame(
                MessageType::AuthenticateResponse,
                requestResult.requestId,
                TaskId{0},
                responsePayload.payload);
        serverSocket->write(responseFrame.encodedFrame);

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);

        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        QCOMPARE(rejectedSpy.count(), 1);

        QCOMPARE(qvariant_cast<AuthenticationStatus>(rejectedSpy.takeFirst().at(0)), authenticationStatus);
    }

    void errorResponse_duringAuthentication_returnsLockedAndReportsRemoteError()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const RequestSendResult requestResult = controller.activate(productKey, deviceId);
        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QSignalSpy errorSpy(&controller, &ApplicationController::activationError);
        QSignalSpy rejectedSpy(&controller, &ApplicationController::activationRejected);
        QVERIFY(errorSpy.isValid());
        QVERIFY(rejectedSpy.isValid());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult request = requestParser.tryTakeFrame();
        QCOMPARE(request.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(request.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(request.frame.header.requestId, requestResult.requestId);

        const ErrorResponseData errorData{ErrorCode::InternalServerError, QStringLiteral("Authentication could not be completed."), QJsonObject{}};
        const ErrorResponseEncodeResult errorPayload = serializeErrorResponse(errorData);
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);

        const auto encodedFrame = serializeFrame(
            MessageType::ErrorResponse,
            requestResult.requestId,
            TaskId{0},
            errorPayload.payload);
        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(encodedFrame.encodedFrame), static_cast<qint64>(encodedFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(rejectedSpy.count(), 0);

        const ErrorResponseData receivedError = qvariant_cast<ErrorResponseData>(errorSpy.takeFirst().at(0));
        QCOMPARE(receivedError.errorCode, ErrorCode::InternalServerError);
        QCOMPARE(receivedError.message, errorData.message);
    }

    void authenticateResponse_malformedPayload_returnsLockedAndReportsInvalidResponsePayload()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult requestResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"),
            QStringLiteral("DEVICE-CLIENT"));

        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QSignalSpy failedSpy(&controller, &ApplicationController::activationFailed);
        QSignalSpy rejectedSpy(&controller, &ApplicationController::activationRejected);
        QSignalSpy remoteErrorSpy(&controller, &ApplicationController::activationError);
        QVERIFY(failedSpy.isValid());
        QVERIFY(rejectedSpy.isValid());
        QVERIFY(remoteErrorSpy.isValid());

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        serverSocket->readAll();

        const QByteArray malformedPayload = QByteArrayLiteral("{not-valid-auth-response");
        const auto encodedFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            requestResult.requestId,
            TaskId{0},
            malformedPayload);

        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(encodedFrame.encodedFrame), static_cast<qint64>(encodedFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(rejectedSpy.count(), 0);
        QCOMPARE(remoteErrorSpy.count(), 0);
        QCOMPARE(
            qvariant_cast<RequestDispatchError>(failedSpy.takeFirst().at(0)),
            RequestDispatchError::InvalidResponsePayload);
    }

    void serverDisconnect_afterActivation_returnsLockedAndDeniesFeatureAccess()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;

        QSignalSpy stateSpy(&controller, &ApplicationController::accessStateChanged);
        QVERIFY(stateSpy.isValid());

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult requestResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"),
            QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        serverSocket->readAll();

        const auto responsePayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(responsePayload.status, AuthenticationEncodeResult::Status::Success);
        const auto encodedFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            requestResult.requestId,
            TaskId{0},
            responsePayload.payload);
        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(encodedFrame.encodedFrame), static_cast<qint64>(encodedFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        serverSocket->disconnectFromHost();
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::UnconnectedState);
        QTRY_VERIFY(!controller.isConnected());

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(!controller.isConnected());

        QCOMPARE(stateSpy.count(), 3);
        QCOMPARE(
            qvariant_cast<ClientAccessState>(stateSpy.at(2).at(0)),
            ClientAccessState::Locked);
    }

    void authenticationRequest_whenDeadlineExpires_returnsLockedAndReportsRequestTimeout()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller(50);

        QSignalSpy failedSpy(&controller, &ApplicationController::activationFailed);
        QVERIFY(failedSpy.isValid());

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult requestResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"),
            QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        serverSocket->readAll();

        QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1000);
        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());
        QCOMPARE(
            qvariant_cast<RequestDispatchError>(failedSpy.takeFirst().at(0)),
            RequestDispatchError::RequestTimeout);
    }

    void activate_afterInvalidKey_canRetryAndBecomeActiveWithoutReconnect()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;

        QSignalSpy stateSpy(&controller, &ApplicationController::accessStateChanged);
        QVERIFY(stateSpy.isValid());

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));

        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const RequestSendResult firstRequest = controller.activate(
            QStringLiteral("MCLD-FFFF-FFFF-FFFF-FFFF"),
            deviceId);

        QCOMPARE(firstRequest.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser firstRequestParser;
        firstRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult firstFrame = firstRequestParser.tryTakeFrame();
        QCOMPARE(firstFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstFrame.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(firstFrame.frame.header.requestId, firstRequest.requestId);

        const auto firstResponsePayload = serializeAuthenticateResponse({AuthenticationStatus::InvalidKey});
        QCOMPARE(firstResponsePayload.status, AuthenticationEncodeResult::Status::Success);
        const auto firstResponseFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            firstRequest.requestId,
            TaskId{0},
            firstResponsePayload.payload);
        QCOMPARE(firstResponseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(firstResponseFrame.encodedFrame), static_cast<qint64>(firstResponseFrame.encodedFrame.size()));
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        const QString validProductKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const RequestSendResult secondRequest = controller.activate(validProductKey, deviceId);
        QCOMPARE(secondRequest.status, RequestSendStatus::Accepted);
        QVERIFY(secondRequest.requestId != firstRequest.requestId);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser secondRequestParser;
        secondRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult secondFrame = secondRequestParser.tryTakeFrame();
        QCOMPARE(secondFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondFrame.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(secondFrame.frame.header.requestId, secondRequest.requestId);

        const auto secondRequestPayload = deserializeAuthenticateRequest(secondFrame.frame.payload);
        QCOMPARE(secondRequestPayload.status, AuthenticateRequestDecodeResult::Status::Success);
        QCOMPARE(secondRequestPayload.data.productKey, validProductKey);
        QCOMPARE(secondRequestPayload.data.deviceId, deviceId);

        const auto secondResponsePayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(secondResponsePayload.status, AuthenticationEncodeResult::Status::Success);
        const auto secondResponseFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            secondRequest.requestId,
            TaskId{0},
            secondResponsePayload.payload);
        QCOMPARE(secondResponseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(secondResponseFrame.encodedFrame), static_cast<qint64>(secondResponseFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        QCOMPARE(stateSpy.count(), 4);
        QCOMPARE(qvariant_cast<ClientAccessState>(stateSpy.at(0).at(0)), ClientAccessState::Authenticating);
        QCOMPARE(qvariant_cast<ClientAccessState>(stateSpy.at(1).at(0)), ClientAccessState::Locked);
        QCOMPARE(qvariant_cast<ClientAccessState>(stateSpy.at(2).at(0)), ClientAccessState::Authenticating);
        QCOMPARE(qvariant_cast<ClientAccessState>(stateSpy.at(3).at(0)), ClientAccessState::Active);
    }

    void activate_whileAuthenticationPending_returnsInvalidRequestWithoutSendingSecondFrame()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const QString firstProductKey = QStringLiteral("MCLD-FFFF-FFFF-FFFF-FFFF");
        const QString secondProductKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const RequestSendResult firstRequest = controller.activate(firstProductKey, deviceId);
        QCOMPARE(firstRequest.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser parser;
        parser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult firstFrame = parser.tryTakeFrame();
        QCOMPARE(firstFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstFrame.frame.header.requestId, firstRequest.requestId);
        QCOMPARE(serverSocket->bytesAvailable(), qint64(0));

        const RequestSendResult secondRequest = controller.activate(secondProductKey, deviceId);
        QCOMPARE(secondRequest.status, RequestSendStatus::Failed);
        QCOMPARE(secondRequest.requestId, RequestId{0});
        QCOMPARE(secondRequest.errorCode, RequestDispatchError::InvalidRequest);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QTest::qWait(100);
        QCOMPARE(serverSocket->bytesAvailable(), qint64(0));

        const auto responsePayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(responsePayload.status, AuthenticationEncodeResult::Status::Success);
        const auto responseFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            firstRequest.requestId,
            TaskId{0},
            responsePayload.payload);
        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(responseFrame.encodedFrame), static_cast<qint64>(responseFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void activate_blankRequiredField_returnsInvalidRequestWithoutSendingFrame_data()
    {
        QTest::addColumn<QString>("productKey");
        QTest::addColumn<QString>("deviceId");

        const QString validKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString validDevice = QStringLiteral("DEVICE-CLIENT");

        QTest::newRow("empty-product-key") << QString() << validDevice;
        QTest::newRow("blank-product-key") << QStringLiteral("   ") << validDevice;
        QTest::newRow("empty-device-id") << validKey << QString();
        QTest::newRow("blank-device-id") << validKey << QStringLiteral("   ");
    }

    void activate_blankRequiredField_returnsInvalidRequestWithoutSendingFrame()
    {
        QFETCH(QString, productKey);
        QFETCH(QString, deviceId);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QCOMPARE(serverSocket->bytesAvailable(), qint64(0));

        const RequestSendResult result = controller.activate(productKey, deviceId);
        QCOMPARE(result.status, RequestSendStatus::Failed);
        QCOMPARE(result.requestId, RequestId{0});
        QCOMPARE(result.errorCode, RequestDispatchError::InvalidRequest);
        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());

        QTest::qWait(100);
        QCOMPARE(serverSocket->bytesAvailable(), qint64(0));
    }

    void serverDisconnect_whileAuthenticating_returnsLockedAndReportsConnectionLostOnce()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QSignalSpy failedSpy(&controller, &ApplicationController::activationFailed);
        QSignalSpy rejectedSpy(&controller, &ApplicationController::activationRejected);
        QSignalSpy remoteErrorSpy(&controller, &ApplicationController::activationError);
        QVERIFY(failedSpy.isValid());
        QVERIFY(rejectedSpy.isValid());
        QVERIFY(remoteErrorSpy.isValid());

        const RequestSendResult requestResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"),
            QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(requestResult.status, RequestSendStatus::Accepted);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        serverSocket->readAll();
        serverSocket->abort();

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QTRY_COMPARE(failedSpy.count(), 1);
        QCOMPARE(
            qvariant_cast<RequestDispatchError>(failedSpy.takeFirst().at(0)),
            RequestDispatchError::ConnectionLost);
        QCOMPARE(rejectedSpy.count(), 0);
        QCOMPARE(remoteErrorSpy.count(), 0);

        QTest::qWait(100);
        QCOMPARE(failedSpy.count(), 0);
    }

    void reconnect_afterActivation_requiresAndAcceptsNewAuthentication()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const QString deviceId = QStringLiteral("DEVICE-CLIENT");

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocketA = server.nextPendingConnection();
        QVERIFY(serverSocketA != nullptr);

        const RequestSendResult firstRequest = controller.activate(productKey, deviceId);
        QCOMPARE(firstRequest.status, RequestSendStatus::Accepted);
        QTRY_VERIFY(serverSocketA->bytesAvailable() > 0);
        serverSocketA->readAll();

        const auto firstResponsePayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(firstResponsePayload.status, AuthenticationEncodeResult::Status::Success);
        const auto firstResponseFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            firstRequest.requestId,
            TaskId{0},
            firstResponsePayload.payload);
        QCOMPARE(firstResponseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocketA->write(firstResponseFrame.encodedFrame), static_cast<qint64>(firstResponseFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        const RequestId firstRequestId = firstRequest.requestId;

        serverSocketA->abort();
        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocketB = server.nextPendingConnection();
        QVERIFY(serverSocketB != nullptr);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QCOMPARE(serverSocketB->bytesAvailable(), qint64(0));

        const RequestSendResult secondRequest = controller.activate(productKey, deviceId);
        QCOMPARE(secondRequest.status, RequestSendStatus::Accepted);
        QVERIFY(secondRequest.requestId != firstRequestId);
        QCOMPARE(controller.accessState(), ClientAccessState::Authenticating);

        QTRY_VERIFY(serverSocketB->bytesAvailable() > 0);
        FrameParser secondRequestParser;
        secondRequestParser.appendData(serverSocketB->readAll());
        const FrameParser::FrameParseResult secondFrame = secondRequestParser.tryTakeFrame();
        QCOMPARE(secondFrame.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondFrame.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(secondFrame.frame.header.requestId, secondRequest.requestId);

        const auto secondResponsePayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(secondResponsePayload.status, AuthenticationEncodeResult::Status::Success);
        const auto secondResponseFrame = serializeFrame(
            MessageType::AuthenticateResponse,
            secondRequest.requestId,
            TaskId{0},
            secondResponsePayload.payload);
        QCOMPARE(secondResponseFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocketB->write(secondResponseFrame.encodedFrame), static_cast<qint64>(secondResponseFrame.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QVERIFY(controller.isConnected());
    }

    void connectionLifecycle_connectThenDisconnect_reportsStateAndKeepsFeaturesLocked()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QSignalSpy connectionSpy(&controller, &ApplicationController::connectionStateChanged);
        QVERIFY(connectionSpy.isValid());
        QVERIFY(!controller.isConnected());

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTRY_COMPARE(connectionSpy.count(), 1);

        QCOMPARE(connectionSpy.at(0).at(0).toBool(), true);
        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        controller.disconnectFromServer();

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(connectionSpy.count(), 2);
        QCOMPARE(connectionSpy.at(1).at(0).toBool(), false);

        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
    }
};

QTEST_GUILESS_MAIN(ApplicationControllerTest)
#include "applicationcontrollertest.moc"
