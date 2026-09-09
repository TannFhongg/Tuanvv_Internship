#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "applicationcontroller.h"
#include "authentication.h"
#include "errorresponse.h"
#include "fileprotocol.h"
#include "frameparser.h"
#include "protocolcodec.h"
#include "protocolconstants.h"
#include "protocoltypes.h"
#include "requesttypes.h"

using MiniCloud::Client::ClientAccessState;
using MiniCloud::Client::RequestDispatchError;
using MiniCloud::Client::RequestSendResult;
using MiniCloud::Client::RequestSendStatus;
using MiniCloud::Protocol::AuthenticateRequestDecodeResult;
using MiniCloud::Protocol::AuthenticationEncodeResult;
using MiniCloud::Protocol::AuthenticationStatus;
using MiniCloud::Protocol::BrowseRequestDecodeResult;
using MiniCloud::Protocol::CreateDirectoryRequestDecodeResult;
using MiniCloud::Protocol::DeleteRequestDecodeResult;
using MiniCloud::Protocol::deserializeAuthenticateRequest;
using MiniCloud::Protocol::deserializeBrowseRequest;
using MiniCloud::Protocol::deserializeCreateDirectoryRequest;
using MiniCloud::Protocol::deserializeDeleteRequest;
using MiniCloud::Protocol::deserializeDownloadRequest;
using MiniCloud::Protocol::deserializeFileChunk;
using MiniCloud::Protocol::deserializeMoveRequest;
using MiniCloud::Protocol::deserializeRenameRequest;
using MiniCloud::Protocol::deserializeSearchRequest;
using MiniCloud::Protocol::deserializeUploadStartRequest;
using MiniCloud::Protocol::DownloadRequestDecodeResult;
using MiniCloud::Protocol::ErrorCode;
using MiniCloud::Protocol::ErrorResponseData;
using MiniCloud::Protocol::ErrorResponseEncodeResult;
using MiniCloud::Protocol::FileChunkDecodeResult;
using MiniCloud::Protocol::FileEntryData;
using MiniCloud::Protocol::FileEntryType;
using MiniCloud::Protocol::FrameEncodeStatus;
using MiniCloud::Protocol::FrameParser;
using MiniCloud::Protocol::MessageType;
using MiniCloud::Protocol::MoveRequestDecodeResult;
using MiniCloud::Protocol::RenameRequestDecodeResult;
using MiniCloud::Protocol::RequestId;
using MiniCloud::Protocol::SearchRequestDecodeResult;
using MiniCloud::Protocol::serializeAuthenticateResponse;
using MiniCloud::Protocol::serializeBrowseResponse;
using MiniCloud::Protocol::serializeDownloadStartResponse;
using MiniCloud::Protocol::serializeErrorResponse;
using MiniCloud::Protocol::serializeFileChunk;
using MiniCloud::Protocol::serializeFileOperationResponse;
using MiniCloud::Protocol::serializeFrame;
using MiniCloud::Protocol::serializeSearchResponse;
using MiniCloud::Protocol::serializeUploadReadyResponse;
using MiniCloud::Protocol::TaskId;
using MiniCloud::Protocol::UploadStartRequestDecodeResult;
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
        qRegisterMetaType<FileEntryData>();
        qRegisterMetaType<QList<FileEntryData>>();
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

    void browse_afterAuthentication_sendsCorrelatedRequestAndEmitsEntries()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);
        QCOMPARE(authenticationRequest.frame.header.requestId, activationResult.requestId);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy browseSpy(&controller, &ApplicationController::browseReceived);
        QVERIFY(browseSpy.isValid());

        QVERIFY(controller.requestBrowse(QStringLiteral("/")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser browseRequestParser;
        browseRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult browseRequest = browseRequestParser.tryTakeFrame();
        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);
        QCOMPARE(browseRequest.frame.header.taskId, TaskId{0});
        const RequestId browseRequestId = browseRequest.frame.header.requestId;
        QVERIFY(browseRequestId != RequestId{0});

        const BrowseRequestDecodeResult decodedRequest = deserializeBrowseRequest(browseRequest.frame.payload);
        QCOMPARE(decodedRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, QStringLiteral("/"));

        const QList<FileEntryData> expectedEntries{
            {QStringLiteral("Documents"),
             QStringLiteral("/Documents"),
             FileEntryType::Directory,
             0,
             quint64{1788424496000ULL}},
            {QStringLiteral("report.txt"),
             QStringLiteral("/report.txt"),
             FileEntryType::File,
             42,
             quint64{1788510896000ULL}}};
        const auto browseResponsePayload = serializeBrowseResponse({QStringLiteral("/"), expectedEntries});

        QCOMPARE(browseResponsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto browseResponse = serializeFrame(
            MessageType::BrowseResponse,
            browseRequestId,
            browseRequest.frame.header.taskId,
            browseResponsePayload.payload);

        QCOMPARE(browseResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(browseResponse.encodedFrame), static_cast<qint64>(browseResponse.encodedFrame.size()));

        QTRY_COMPARE(browseSpy.count(), 1);
        const QList<QVariant> browseArguments = browseSpy.takeFirst();
        QCOMPARE(browseArguments.size(), 2);
        QCOMPARE(browseArguments.at(0).toString(), QStringLiteral("/"));

        const QList<FileEntryData> actualEntries = qvariant_cast<QList<FileEntryData>>(browseArguments.at(1));
        QCOMPARE(actualEntries.size(), expectedEntries.size());

        for (qsizetype index = 0; index < expectedEntries.size(); ++index)
        {
            const FileEntryData &actual = actualEntries.at(index);
            const FileEntryData &expected = expectedEntries.at(index);
            QCOMPARE(actual.name, expected.name);
            QCOMPARE(actual.path, expected.path);
            QCOMPARE(actual.type, expected.type);
            QCOMPARE(actual.sizeBytes, expected.sizeBytes);
            QCOMPARE(actual.lastModifiedUtcMs, expected.lastModifiedUtcMs);
        }

        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void browse_beforeAuthentication_failsLocallyWithoutSendingFrame()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        QSignalSpy browseFailedSpy(&controller, &ApplicationController::browseFailed);
        QVERIFY(browseFailedSpy.isValid());
        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        QVERIFY(!controller.requestBrowse(QStringLiteral("/")));

        QCOMPARE(browseFailedSpy.count(), 1);
        QCOMPARE(
            browseFailedSpy.at(0).at(0).toString(),
            QStringLiteral("Session must be authenticated before browsing files."));

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
    }

    void browse_responseWithUnexpectedRequestId_isIgnoredUntilCorrelatedResponseArrives()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy browseSpy(&controller, &ApplicationController::browseReceived);
        QVERIFY(browseSpy.isValid());

        QVERIFY(controller.requestBrowse(QStringLiteral("/")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser browseRequestParser;
        browseRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult browseRequest = browseRequestParser.tryTakeFrame();

        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);
        const RequestId browseRequestId = browseRequest.frame.header.requestId;
        QVERIFY(browseRequestId != RequestId{0});

        const QList<FileEntryData> expectedEntries{
            {QStringLiteral("Documents"),
             QStringLiteral("/Documents"),
             FileEntryType::Directory,
             0,
             quint64{1788424496000ULL}},

            {QStringLiteral("report.txt"),
             QStringLiteral("/report.txt"),
             FileEntryType::File,
             42,
             quint64{1788510896000ULL}}};
        const auto browseResponsePayload = serializeBrowseResponse({QStringLiteral("/"), expectedEntries});
        QCOMPARE(browseResponsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto unexpectedResponse = serializeFrame(
            MessageType::BrowseResponse,
            browseRequestId + RequestId{1},
            browseRequest.frame.header.taskId,
            browseResponsePayload.payload);
        QCOMPARE(unexpectedResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(unexpectedResponse.encodedFrame),
            static_cast<qint64>(unexpectedResponse.encodedFrame.size()));

        QTest::qWait(100);
        QCOMPARE(browseSpy.count(), 0);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        const auto correlatedResponse = serializeFrame(
            MessageType::BrowseResponse,
            browseRequestId,
            browseRequest.frame.header.taskId,
            browseResponsePayload.payload);

        QCOMPARE(correlatedResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(correlatedResponse.encodedFrame),
            static_cast<qint64>(correlatedResponse.encodedFrame.size()));

        QTRY_COMPARE(browseSpy.count(), 1);
        const QList<QVariant> browseArguments = browseSpy.takeFirst();
        QCOMPARE(browseArguments.at(0).toString(), QStringLiteral("/"));

        const QList<FileEntryData> actualEntries =
            qvariant_cast<QList<FileEntryData>>(browseArguments.at(1));
        QCOMPARE(actualEntries.size(), expectedEntries.size());

        for (qsizetype index = 0; index < expectedEntries.size(); ++index)
        {
            const FileEntryData &actual = actualEntries.at(index);
            const FileEntryData &expected = expectedEntries.at(index);
            QCOMPARE(actual.name, expected.name);
            QCOMPARE(actual.path, expected.path);
            QCOMPARE(actual.type, expected.type);
            QCOMPARE(actual.sizeBytes, expected.sizeBytes);
            QCOMPARE(actual.lastModifiedUtcMs, expected.lastModifiedUtcMs);
        }

        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void browse_errorResponseForActiveRequest_reportsFileErrorAndKeepsSessionValid()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy browseReceivedSpy(&controller, &ApplicationController::browseReceived);
        QVERIFY(browseReceivedSpy.isValid());
        QSignalSpy browseFailedSpy(&controller, &ApplicationController::browseFailed);
        QVERIFY(browseFailedSpy.isValid());

        QVERIFY(controller.requestBrowse(QStringLiteral("/missing")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser browseRequestParser;
        browseRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult browseRequest = browseRequestParser.tryTakeFrame();
        QCOMPARE(browseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseRequest.frame.header.messageType, MessageType::BrowseRequest);

        const BrowseRequestDecodeResult decodedRequest = deserializeBrowseRequest(browseRequest.frame.payload);
        QCOMPARE(decodedRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, QStringLiteral("/missing"));

        const ErrorResponseEncodeResult errorPayload = serializeErrorResponse(
            {ErrorCode::FileNotFound, QStringLiteral("The requested file was not found.")});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);

        const auto errorResponse = serializeFrame(
            MessageType::ErrorResponse,
            browseRequest.frame.header.requestId,
            browseRequest.frame.header.taskId,
            errorPayload.payload);

        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(errorResponse.encodedFrame),
            static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_COMPARE(browseFailedSpy.count(), 1);
        QCOMPARE(
            browseFailedSpy.at(0).at(0).toString(), QStringLiteral("The requested file was not found."));
        QCOMPARE(browseReceivedSpy.count(), 0);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestBrowse(QStringLiteral("/")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser retryBrowseRequestParser;
        retryBrowseRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult retryBrowseRequest = retryBrowseRequestParser.tryTakeFrame();
        QCOMPARE(retryBrowseRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(retryBrowseRequest.frame.header.messageType, MessageType::BrowseRequest);
        QVERIFY(retryBrowseRequest.frame.header.requestId != browseRequest.frame.header.requestId);

        const BrowseRequestDecodeResult decodedRetryRequest = deserializeBrowseRequest(retryBrowseRequest.frame.payload);
        QCOMPARE(decodedRetryRequest.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRetryRequest.data.path, QStringLiteral("/"));
    }

    void search_afterAuthentication_sendsCorrelatedRequestAndEmitsEntries()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy searchSpy(&controller, &ApplicationController::searchReceived);
        QVERIFY(searchSpy.isValid());

        QVERIFY(controller.requestSearch(
            QStringLiteral("/Documents"), QStringLiteral("report")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser searchRequestParser;
        searchRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult searchRequest = searchRequestParser.tryTakeFrame();
        QCOMPARE(searchRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(searchRequest.frame.header.messageType, MessageType::SearchRequest);
        QCOMPARE(searchRequest.frame.header.taskId, TaskId{0});
        const RequestId searchRequestId = searchRequest.frame.header.requestId;
        QVERIFY(searchRequestId != RequestId{0});

        const SearchRequestDecodeResult decodedRequest = deserializeSearchRequest(searchRequest.frame.payload);
        QCOMPARE(decodedRequest.status, SearchRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, QStringLiteral("/Documents"));
        QCOMPARE(decodedRequest.data.query, QStringLiteral("report"));

        const QList<FileEntryData> expectedEntries{
            {QStringLiteral("Reports"),
             QStringLiteral("/Documents/Reports"),
             FileEntryType::Directory,
             0,
             quint64{1788424496000ULL}},
            {QStringLiteral("report.txt"),
             QStringLiteral("/Documents/report.txt"),
             FileEntryType::File,
             42,
             quint64{1788510896000ULL}}};
        const auto searchResponsePayload = serializeSearchResponse({QStringLiteral("/Documents"), expectedEntries});
        QCOMPARE(searchResponsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto searchResponse = serializeFrame(
            MessageType::SearchResponse,
            searchRequestId,
            searchRequest.frame.header.taskId,
            searchResponsePayload.payload);

        QCOMPARE(searchResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(searchResponse.encodedFrame),
            static_cast<qint64>(searchResponse.encodedFrame.size()));

        QTRY_COMPARE(searchSpy.count(), 1);
        const QList<QVariant> searchArguments = searchSpy.takeFirst();
        QCOMPARE(searchArguments.at(0).toString(), QStringLiteral("/Documents"));

        const QList<FileEntryData> actualEntries = qvariant_cast<QList<FileEntryData>>(searchArguments.at(1));
        QCOMPARE(actualEntries.size(), expectedEntries.size());

        for (qsizetype index = 0; index < expectedEntries.size(); ++index)
        {
            const FileEntryData &actual = actualEntries.at(index);
            const FileEntryData &expected = expectedEntries.at(index);
            QCOMPARE(actual.name, expected.name);
            QCOMPARE(actual.path, expected.path);
            QCOMPARE(actual.type, expected.type);
            QCOMPARE(actual.sizeBytes, expected.sizeBytes);
            QCOMPARE(actual.lastModifiedUtcMs, expected.lastModifiedUtcMs);
        }

        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void createDirectory_afterAuthentication_sendsCorrelatedRequestAndEmitsCreatedPath()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload =
            serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(
            &controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestCreateDirectory(
            QStringLiteral("/"), QStringLiteral("Documents")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser createRequestParser;
        createRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult createRequest = createRequestParser.tryTakeFrame();
        QCOMPARE(createRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(createRequest.frame.header.messageType, MessageType::CreateDirectoryRequest);
        QCOMPARE(createRequest.frame.header.taskId, TaskId{0});
        const RequestId createRequestId = createRequest.frame.header.requestId;
        QVERIFY(createRequestId != RequestId{0});

        const CreateDirectoryRequestDecodeResult decodedRequest =
            deserializeCreateDirectoryRequest(createRequest.frame.payload);
        QCOMPARE(decodedRequest.status, CreateDirectoryRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.parentPath, QStringLiteral("/"));
        QCOMPARE(decodedRequest.data.name, QStringLiteral("Documents"));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            createRequestId,
            createRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(
            completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents"));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void rename_afterAuthentication_sendsCorrelatedRequestAndEmitsUpdatedPath()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestRename(QStringLiteral("/Documents/old.txt"), QStringLiteral("new.txt")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser renameRequestParser;
        renameRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult renameRequest = renameRequestParser.tryTakeFrame();
        QCOMPARE(renameRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(renameRequest.frame.header.messageType, MessageType::RenameRequest);
        QCOMPARE(renameRequest.frame.header.taskId, TaskId{0});
        const RequestId renameRequestId = renameRequest.frame.header.requestId;
        QVERIFY(renameRequestId != RequestId{0});

        const RenameRequestDecodeResult decodedRequest = deserializeRenameRequest(renameRequest.frame.payload);
        QCOMPARE(decodedRequest.status, RenameRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, QStringLiteral("/Documents/old.txt"));
        QCOMPARE(decodedRequest.data.newName, QStringLiteral("new.txt"));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/new.txt")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            renameRequestId,
            renameRequest.frame.header.taskId,
            completionPayload.payload);

        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(
            completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents/new.txt"));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void move_afterAuthentication_sendsCorrelatedRequestAndEmitsUpdatedPath()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestMove(
            QStringLiteral("/Documents/report.txt"), QStringLiteral("/Archive")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser moveRequestParser;
        moveRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult moveRequest = moveRequestParser.tryTakeFrame();
        QCOMPARE(moveRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(moveRequest.frame.header.messageType, MessageType::MoveRequest);
        QCOMPARE(moveRequest.frame.header.taskId, TaskId{0});
        const RequestId moveRequestId = moveRequest.frame.header.requestId;
        QVERIFY(moveRequestId != RequestId{0});

        const MoveRequestDecodeResult decodedRequest = deserializeMoveRequest(moveRequest.frame.payload);
        QCOMPARE(decodedRequest.status, MoveRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.sourcePath, QStringLiteral("/Documents/report.txt"));
        QCOMPARE(decodedRequest.data.destinationDirectoryPath, QStringLiteral("/Archive"));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Archive/report.txt")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            moveRequestId,
            moveRequest.frame.header.taskId,
            completionPayload.payload);

        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(
            completionSpy.at(0).at(0).toString(), QStringLiteral("/Archive/report.txt"));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void delete_afterAuthentication_sendsCorrelatedRequestAndEmitsRemovedPath()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestDelete(QStringLiteral("/Documents/report.txt")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser deleteRequestParser;
        deleteRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult deleteRequest = deleteRequestParser.tryTakeFrame();
        QCOMPARE(deleteRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(deleteRequest.frame.header.messageType, MessageType::DeleteRequest);
        QCOMPARE(deleteRequest.frame.header.taskId, TaskId{0});
        const RequestId deleteRequestId = deleteRequest.frame.header.requestId;
        QVERIFY(deleteRequestId != RequestId{0});

        const DeleteRequestDecodeResult decodedRequest = deserializeDeleteRequest(deleteRequest.frame.payload);
        QCOMPARE(decodedRequest.status, DeleteRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, QStringLiteral("/Documents/report.txt"));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/report.txt")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            deleteRequestId,
            deleteRequest.frame.header.taskId,
            completionPayload.payload);

        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents/report.txt"));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void uploadFile_afterAuthentication_sendsStartSequentialChunksAndEmitsCompletion()
    {
        constexpr qsizetype maximumChunkSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes);
        const QByteArray firstChunk(maximumChunkSize, 'a');
        const QByteArray finalChunk(1, 'b');
        const QByteArray expectedContent = firstChunk + finalChunk;

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QFile localFile(localFilePath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        QCOMPARE(localFile.write(expectedContent), static_cast<qint64>(expectedContent.size()));
        localFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());
        QSignalSpy progressSpy(&controller, &ApplicationController::uploadProgress);
        QVERIFY(progressSpy.isValid());

        QVERIFY(controller.requestUpload(localFilePath, QStringLiteral("/Documents")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser uploadStartRequestParser;
        uploadStartRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult uploadStartRequest = uploadStartRequestParser.tryTakeFrame();
        QCOMPARE(uploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);
        QCOMPARE(uploadStartRequest.frame.header.taskId, TaskId{0});
        const RequestId uploadRequestId = uploadStartRequest.frame.header.requestId;
        QVERIFY(uploadRequestId != RequestId{0});

        const UploadStartRequestDecodeResult decodedStartRequest = deserializeUploadStartRequest(uploadStartRequest.frame.payload);
        QCOMPARE(decodedStartRequest.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedStartRequest.data.destinationDirectoryPath, QStringLiteral("/Documents"));
        QCOMPARE(decodedStartRequest.data.fileName, QStringLiteral("report.bin"));
        QCOMPARE(decodedStartRequest.data.totalSizeBytes, static_cast<quint64>(expectedContent.size()));

        QTest::qWait(100);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});

        const auto readyPayload = serializeUploadReadyResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(readyPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto readyResponse = serializeFrame(
            MessageType::UploadReadyResponse,
            uploadRequestId,
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

        const MiniCloud::Protocol::ProtocolFrame &firstChunkFrame = uploadChunkFrames.at(0);
        QCOMPARE(firstChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(firstChunkFrame.header.requestId, uploadRequestId);
        QCOMPARE(firstChunkFrame.header.taskId, TaskId{0});

        const FileChunkDecodeResult decodedFirstChunk = deserializeFileChunk(firstChunkFrame.payload);
        QCOMPARE(decodedFirstChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedFirstChunk.data.offset, quint64{0});
        QCOMPARE(decodedFirstChunk.data.bytes, firstChunk);

        const MiniCloud::Protocol::ProtocolFrame &finalChunkFrame = uploadChunkFrames.at(1);
        QCOMPARE(finalChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(finalChunkFrame.header.requestId, uploadRequestId);
        QCOMPARE(finalChunkFrame.header.taskId, TaskId{0});

        const FileChunkDecodeResult decodedFinalChunk = deserializeFileChunk(finalChunkFrame.payload);
        QCOMPARE(decodedFinalChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedFinalChunk.data.offset, static_cast<quint64>(firstChunk.size()));
        QCOMPARE(decodedFinalChunk.data.bytes, finalChunk);

        QTRY_COMPARE(progressSpy.count(), 2);

        const QList<QVariant> finalProgress = progressSpy.at(progressSpy.count() - 1);
        QCOMPARE(finalProgress.at(0).toULongLong(), static_cast<quint64>(expectedContent.size()));
        QCOMPARE(finalProgress.at(1).toULongLong(), static_cast<quint64>(expectedContent.size()));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            uploadRequestId,
            TaskId{0},
            completionPayload.payload);

        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents/report.bin"));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void uploadEmptyFile_afterAuthentication_completesWithoutSendingChunks()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("empty.bin"));
        QFile localFile(localFilePath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        localFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());
        QSignalSpy progressSpy(&controller, &ApplicationController::uploadProgress);
        QVERIFY(progressSpy.isValid());

        QVERIFY(controller.requestUpload(localFilePath, QStringLiteral("/Documents")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser uploadStartRequestParser;
        uploadStartRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult uploadStartRequest = uploadStartRequestParser.tryTakeFrame();
        QCOMPARE(uploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);
        QCOMPARE(uploadStartRequest.frame.header.taskId, TaskId{0});
        const RequestId uploadRequestId = uploadStartRequest.frame.header.requestId;
        QVERIFY(uploadRequestId != RequestId{0});

        const UploadStartRequestDecodeResult decodedStartRequest = deserializeUploadStartRequest(uploadStartRequest.frame.payload);
        QCOMPARE(decodedStartRequest.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedStartRequest.data.destinationDirectoryPath, QStringLiteral("/Documents"));
        QCOMPARE(decodedStartRequest.data.fileName, QStringLiteral("empty.bin"));
        QCOMPARE(decodedStartRequest.data.totalSizeBytes, quint64{0});

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/empty.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            uploadRequestId,
            uploadStartRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(
            completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents/empty.bin"));

        QTRY_COMPARE(progressSpy.count(), 1);
        const QList<QVariant> finalProgress = progressSpy.at(0);
        QCOMPARE(finalProgress.at(0).toULongLong(), quint64{0});
        QCOMPARE(finalProgress.at(1).toULongLong(), quint64{0});

        QTest::qWait(100);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void uploadFile_errorResponseToStart_reportsErrorWithoutSendingChunks()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());

        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QFile localFile(localFilePath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        QCOMPARE(localFile.write(QByteArrayLiteral("upload-data")), qint64{11});
        localFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());
        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());

        QVERIFY(controller.requestUpload(localFilePath, QStringLiteral("/Documents")));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser uploadStartRequestParser;
        uploadStartRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult uploadStartRequest = uploadStartRequestParser.tryTakeFrame();
        QCOMPARE(uploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);
        const RequestId firstUploadRequestId = uploadStartRequest.frame.header.requestId;

        const ErrorResponseEncodeResult errorPayload = serializeErrorResponse(
            {ErrorCode::FileNotFound, QStringLiteral("The destination directory does not exist.")});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);

        const auto errorResponse = serializeFrame(
            MessageType::ErrorResponse,
            firstUploadRequestId,
            uploadStartRequest.frame.header.taskId,
            errorPayload.payload);

        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(errorResponse.encodedFrame),
            static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("The destination directory does not exist."));
        QCOMPARE(completionSpy.count(), 0);

        QTest::qWait(100);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestUpload(localFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser restartUploadStartRequestParser;
        restartUploadStartRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult restartUploadStartRequest = restartUploadStartRequestParser.tryTakeFrame();
        QCOMPARE(restartUploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(restartUploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);
        QVERIFY(restartUploadStartRequest.frame.header.requestId != firstUploadRequestId);
    }

    void uploadFile_missingLocalFile_failsLocallyWithoutSendingFrame()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());

        const QString missingLocalFilePath = localDirectory.filePath(QStringLiteral("missing.bin"));
        const QString validLocalFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QFile validLocalFile(validLocalFilePath);
        QVERIFY(validLocalFile.open(QIODevice::WriteOnly));
        QCOMPARE(validLocalFile.write(QByteArrayLiteral("ok")), qint64{2});
        validLocalFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());
        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        QVERIFY(!controller.requestUpload(missingLocalFilePath, QStringLiteral("/Documents")));

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestUpload(validLocalFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser uploadStartRequestParser;
        uploadStartRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult uploadStartRequest = uploadStartRequestParser.tryTakeFrame();
        QCOMPARE(uploadStartRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStartRequest.frame.header.messageType, MessageType::UploadStartRequest);
        QCOMPARE(uploadStartRequest.frame.header.taskId, TaskId{0});
        QVERIFY(uploadStartRequest.frame.header.requestId != RequestId{0});
    }

    void downloadFile_afterAuthentication_writesSequentialChunksAndEmitsCompletion()
    {
        constexpr qsizetype maximumChunkSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes);
        const QByteArray firstChunk(maximumChunkSize, 'a');
        const QByteArray finalChunk(1, 'b');
        const QByteArray expectedContent = firstChunk + finalChunk;

        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QVERIFY(!QFileInfo::exists(localFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());
        QSignalSpy progressSpy(&controller, &ApplicationController::downloadProgress);
        QVERIFY(progressSpy.isValid());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        QCOMPARE(downloadRequest.frame.header.taskId, TaskId{0});
        const RequestId downloadRequestId = downloadRequest.frame.header.requestId;
        QVERIFY(downloadRequestId != RequestId{0});

        const DownloadRequestDecodeResult decodedDownloadRequest = deserializeDownloadRequest(downloadRequest.frame.payload);
        QCOMPARE(decodedDownloadRequest.status, DownloadRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDownloadRequest.data.path, QStringLiteral("/Documents/report.bin"));

        const auto startPayload = serializeDownloadStartResponse(
            {QStringLiteral("/Documents/report.bin"), static_cast<quint64>(expectedContent.size())});

        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto startResponse = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);

        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        const auto firstChunkPayload = serializeFileChunk({quint64{0}, firstChunk});
        QCOMPARE(firstChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto firstChunkFrame = serializeFrame(
            MessageType::FileChunk,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            firstChunkPayload.payload);

        QCOMPARE(firstChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(firstChunkFrame.encodedFrame),
            static_cast<qint64>(firstChunkFrame.encodedFrame.size()));

        const auto finalChunkPayload = serializeFileChunk(
            {static_cast<quint64>(firstChunk.size()), finalChunk});
        QCOMPARE(finalChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto finalChunkFrame = serializeFrame(
            MessageType::FileChunk,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            finalChunkPayload.payload);

        QCOMPARE(finalChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(finalChunkFrame.encodedFrame),
            static_cast<qint64>(finalChunkFrame.encodedFrame.size()));

        QTRY_COMPARE(progressSpy.count(), 2);
        const QList<QVariant> finalProgress = progressSpy.at(progressSpy.count() - 1);
        QCOMPARE(finalProgress.at(0).toULongLong(), static_cast<quint64>(expectedContent.size()));
        QCOMPARE(finalProgress.at(1).toULongLong(), static_cast<quint64>(expectedContent.size()));
        QVERIFY(!QFileInfo::exists(localFilePath));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            completionPayload.payload);

        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents/report.bin"));
        QVERIFY(QFileInfo(localFilePath).isFile());
        QFile downloadedFile(localFilePath);
        QVERIFY(downloadedFile.open(QIODevice::ReadOnly));
        QCOMPARE(downloadedFile.readAll(), expectedContent);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void downloadEmptyFile_afterAuthentication_createsEmptyTargetAndCompletesWithoutChunks()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("empty.bin"));
        QVERIFY(!QFileInfo::exists(localFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());
        QSignalSpy progressSpy(&controller, &ApplicationController::downloadProgress);
        QVERIFY(progressSpy.isValid());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/empty.bin"), localFilePath));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        QCOMPARE(downloadRequest.frame.header.taskId, TaskId{0});
        const RequestId downloadRequestId = downloadRequest.frame.header.requestId;
        QVERIFY(downloadRequestId != RequestId{0});

        const DownloadRequestDecodeResult decodedDownloadRequest = deserializeDownloadRequest(downloadRequest.frame.payload);
        QCOMPARE(decodedDownloadRequest.status, DownloadRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDownloadRequest.data.path, QStringLiteral("/Documents/empty.bin"));

        const auto startPayload = serializeDownloadStartResponse(
            {QStringLiteral("/Documents/empty.bin"), quint64{0}});
        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto startResponse = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);
        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        QTRY_COMPARE(progressSpy.count(), 1);
        const QList<QVariant> progress = progressSpy.at(0);
        QCOMPARE(progress.at(0).toULongLong(), quint64{0});
        QCOMPARE(progress.at(1).toULongLong(), quint64{0});
        QVERIFY(!QFileInfo::exists(localFilePath));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/empty.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(completionSpy.at(0).at(0).toString(), QStringLiteral("/Documents/empty.bin"));
        QVERIFY(QFileInfo(localFilePath).isFile());
        QCOMPARE(QFileInfo(localFilePath).size(), qint64{0});
        QCOMPARE(
            QDir(localDirectory.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{QStringLiteral("empty.bin")});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void downloadFile_unexpectedChunkOffset_reportsErrorAndDiscardsPartialTarget()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QVERIFY(!QFileInfo::exists(localFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());
        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        const RequestId downloadRequestId = downloadRequest.frame.header.requestId;

        const auto startPayload = serializeDownloadStartResponse(
            {QStringLiteral("/Documents/report.bin"), quint64{5}});
        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto startResponse = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);
        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        const auto invalidChunkPayload = serializeFileChunk({quint64{1}, QByteArrayLiteral("abc")});
        QCOMPARE(invalidChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto invalidChunkFrame = serializeFrame(
            MessageType::FileChunk,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            invalidChunkPayload.payload);
        QCOMPARE(invalidChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(invalidChunkFrame.encodedFrame),
            static_cast<qint64>(invalidChunkFrame.encodedFrame.size()));

        QTRY_COMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
        QCOMPARE(completionSpy.count(), 0);
        QVERIFY(!QFileInfo::exists(localFilePath));
        QCOMPARE(
            QDir(localDirectory.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser retryRequestParser;
        retryRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult retryRequest = retryRequestParser.tryTakeFrame();
        QCOMPARE(retryRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(retryRequest.frame.header.messageType, MessageType::DownloadRequest);
        QVERIFY(retryRequest.frame.header.requestId != downloadRequestId);
    }

    void downloadFile_completionBeforeDeclaredSize_reportsErrorAndDiscardsPartialTarget()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QVERIFY(!QFileInfo::exists(localFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);

        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());
        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        const RequestId downloadRequestId = downloadRequest.frame.header.requestId;

        const auto startPayload = serializeDownloadStartResponse(
            {QStringLiteral("/Documents/report.bin"), quint64{5}});
        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto startResponse = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);
        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        const auto partialChunkPayload = serializeFileChunk({quint64{0}, QByteArrayLiteral("abc")});
        QCOMPARE(partialChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto partialChunkFrame = serializeFrame(
            MessageType::FileChunk,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            partialChunkPayload.payload);
        QCOMPARE(partialChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(partialChunkFrame.encodedFrame),
            static_cast<qint64>(partialChunkFrame.encodedFrame.size()));

        const auto completionPayload = serializeFileOperationResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(completionPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto completionResponse = serializeFrame(
            MessageType::FileOperationResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            completionPayload.payload);
        QCOMPARE(completionResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(completionResponse.encodedFrame),
            static_cast<qint64>(completionResponse.encodedFrame.size()));

        QTRY_COMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
        QCOMPARE(completionSpy.count(), 0);
        QVERIFY(!QFileInfo::exists(localFilePath));
        QCOMPARE(
            QDir(localDirectory.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void downloadFile_existingLocalTarget_failsLocallyWithoutSendingFrame()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QFile existingFile(localFilePath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        QCOMPARE(existingFile.write(QByteArrayLiteral("existing-local-data")), qint64{19});
        existingFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());
        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        QVERIFY(!controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QVERIFY(QFileInfo(localFilePath).isFile());
        QCOMPARE(QFileInfo(localFilePath).size(), qint64{19});
        QVERIFY(existingFile.open(QIODevice::ReadOnly));
        QCOMPARE(existingFile.readAll(), QByteArrayLiteral("existing-local-data"));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
    }

    void downloadFile_errorResponseDuringTransfer_reportsErrorAndDiscardsPartialTarget()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QVERIFY(!QFileInfo::exists(localFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());
        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        const RequestId downloadRequestId = downloadRequest.frame.header.requestId;

        const auto startPayload = serializeDownloadStartResponse(
            {QStringLiteral("/Documents/report.bin"), quint64{5}});
        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto startResponse = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);
        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        const auto partialChunkPayload = serializeFileChunk({quint64{0}, QByteArrayLiteral("abc")});
        QCOMPARE(partialChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto partialChunkFrame = serializeFrame(
            MessageType::FileChunk,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            partialChunkPayload.payload);
        QCOMPARE(partialChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(partialChunkFrame.encodedFrame),
            static_cast<qint64>(partialChunkFrame.encodedFrame.size()));

        const ErrorResponseEncodeResult errorPayload = serializeErrorResponse(
            {ErrorCode::InternalServerError, QStringLiteral("The server could not finish the download.")});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);
        const auto errorResponse = serializeFrame(
            MessageType::ErrorResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            errorPayload.payload);
        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(errorResponse.encodedFrame),
            static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("The server could not finish the download."));
        QCOMPARE(completionSpy.count(), 0);
        QVERIFY(!QFileInfo::exists(localFilePath));
        QCOMPARE(
            QDir(localDirectory.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser retryRequestParser;
        retryRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult retryRequest = retryRequestParser.tryTakeFrame();
        QCOMPARE(retryRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(retryRequest.frame.header.messageType, MessageType::DownloadRequest);
        QVERIFY(retryRequest.frame.header.requestId != downloadRequestId);
    }

    void serverDisconnect_duringDownload_discardsPartialTargetResetsSessionAndReportsConnectionLostOnce()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QVERIFY(!QFileInfo::exists(localFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());
        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(completionSpy.isValid());
        QSignalSpy progressSpy(&controller, &ApplicationController::downloadProgress);
        QVERIFY(progressSpy.isValid());
        QSignalSpy connectionStateSpy(&controller, &ApplicationController::connectionStateChanged);
        QVERIFY(connectionStateSpy.isValid());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), localFilePath));

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        const RequestId downloadRequestId = downloadRequest.frame.header.requestId;

        const auto startPayload = serializeDownloadStartResponse(
            {QStringLiteral("/Documents/report.bin"), quint64{5}});
        QCOMPARE(startPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto startResponse = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            startPayload.payload);
        QCOMPARE(startResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(startResponse.encodedFrame),
            static_cast<qint64>(startResponse.encodedFrame.size()));

        const auto partialChunkPayload = serializeFileChunk({quint64{0}, QByteArrayLiteral("abc")});
        QCOMPARE(partialChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto partialChunkFrame = serializeFrame(
            MessageType::FileChunk,
            downloadRequestId,
            downloadRequest.frame.header.taskId,
            partialChunkPayload.payload);
        QCOMPARE(partialChunkFrame.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(partialChunkFrame.encodedFrame),
            static_cast<qint64>(partialChunkFrame.encodedFrame.size()));

        QTRY_COMPARE(progressSpy.count(), 1);
        QCOMPARE(progressSpy.at(0).at(0).toULongLong(), quint64{3});
        QCOMPARE(progressSpy.at(0).at(1).toULongLong(), quint64{5});
        QVERIFY(!QFileInfo::exists(localFilePath));

        serverSocket->abort();

        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("The download connection was lost."));
        QCOMPARE(completionSpy.count(), 0);
        QCOMPARE(connectionStateSpy.count(), 1);
        QCOMPARE(connectionStateSpy.at(0).at(0).toBool(), false);
        QVERIFY(!QFileInfo::exists(localFilePath));
        QCOMPARE(
            QDir(localDirectory.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{});
        QTest::qWait(100);
        QCOMPARE(errorSpy.count(), 1);
    }

    void serverDisconnect_whileUploadAwaitingCompletion_resetsTransferAndLocksSession()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString localFilePath = localDirectory.filePath(QStringLiteral("report.bin"));
        QFile localFile(localFilePath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        QCOMPARE(localFile.write(QByteArrayLiteral("abcde")), qint64{5});
        localFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *firstServerSocket = server.nextPendingConnection();
        QVERIFY(firstServerSocket != nullptr);

        const RequestSendResult firstActivation = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(firstActivation.status, RequestSendStatus::Accepted);
        QTRY_VERIFY(firstServerSocket->bytesAvailable() > 0);

        FrameParser firstAuthenticationParser;
        firstAuthenticationParser.appendData(firstServerSocket->readAll());
        const FrameParser::FrameParseResult firstAuthentication = firstAuthenticationParser.tryTakeFrame();
        QCOMPARE(firstAuthentication.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstAuthentication.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult validAuthentication = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(validAuthentication.status, AuthenticationEncodeResult::Status::Success);
        const auto firstAuthenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            firstActivation.requestId,
            TaskId{0},
            validAuthentication.payload);
        QCOMPARE(firstAuthenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(firstServerSocket->write(firstAuthenticationResponse.encodedFrame), static_cast<qint64>(firstAuthenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QSignalSpy completionSpy(&controller, &ApplicationController::fileOperationCompleted);
        QVERIFY(errorSpy.isValid());
        QVERIFY(completionSpy.isValid());

        QVERIFY(controller.requestUpload(localFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(firstServerSocket->bytesAvailable() > 0);

        FrameParser uploadStartParser;
        uploadStartParser.appendData(firstServerSocket->readAll());
        const FrameParser::FrameParseResult uploadStart = uploadStartParser.tryTakeFrame();
        QCOMPARE(uploadStart.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStart.frame.header.messageType, MessageType::UploadStartRequest);
        const RequestId uploadRequestId = uploadStart.frame.header.requestId;

        const UploadStartRequestDecodeResult decodedUploadStart = deserializeUploadStartRequest(uploadStart.frame.payload);
        QCOMPARE(decodedUploadStart.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedUploadStart.data.destinationDirectoryPath, QStringLiteral("/Documents"));
        QCOMPARE(decodedUploadStart.data.fileName, QStringLiteral("report.bin"));
        QCOMPARE(decodedUploadStart.data.totalSizeBytes, quint64{5});

        const auto readyPayload = serializeUploadReadyResponse({QStringLiteral("/Documents/report.bin")});
        QCOMPARE(readyPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);
        const auto readyResponse = serializeFrame(
            MessageType::UploadReadyResponse,
            uploadRequestId,
            uploadStart.frame.header.taskId,
            readyPayload.payload);
        QCOMPARE(readyResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(firstServerSocket->write(readyResponse.encodedFrame), static_cast<qint64>(readyResponse.encodedFrame.size()));

        QTRY_VERIFY(firstServerSocket->bytesAvailable() > 0);
        FrameParser chunkParser;
        chunkParser.appendData(firstServerSocket->readAll());
        const FrameParser::FrameParseResult chunk = chunkParser.tryTakeFrame();
        QCOMPARE(chunk.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(chunk.frame.header.messageType, MessageType::FileChunk);
        QCOMPARE(chunk.frame.header.requestId, uploadRequestId);

        const FileChunkDecodeResult decodedChunk = deserializeFileChunk(chunk.frame.payload);
        QCOMPARE(decodedChunk.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decodedChunk.data.offset, quint64{0});
        QCOMPARE(decodedChunk.data.bytes, QByteArrayLiteral("abcde"));

        firstServerSocket->abort();
        QTRY_VERIFY(!controller.isConnected());
        QTRY_COMPARE(controller.accessState(), ClientAccessState::Locked);
        QVERIFY(!controller.isFeatureAccessAllowed());
        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("The upload connection was lost."));
        QCOMPARE(completionSpy.count(), 0);
        QTest::qWait(100);
        QCOMPARE(errorSpy.count(), 1);

        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *secondServerSocket = server.nextPendingConnection();
        QVERIFY(secondServerSocket != nullptr);

        const RequestSendResult secondActivation = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(secondActivation.status, RequestSendStatus::Accepted);
        QTRY_VERIFY(secondServerSocket->bytesAvailable() > 0);

        FrameParser secondAuthenticationParser;
        secondAuthenticationParser.appendData(secondServerSocket->readAll());
        const FrameParser::FrameParseResult secondAuthentication = secondAuthenticationParser.tryTakeFrame();
        QCOMPARE(secondAuthentication.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondAuthentication.frame.header.messageType, MessageType::AuthenticateRequest);

        const auto secondAuthenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            secondActivation.requestId,
            TaskId{0},
            validAuthentication.payload);
        QCOMPARE(secondAuthenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(secondServerSocket->write(secondAuthenticationResponse.encodedFrame), static_cast<qint64>(secondAuthenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QVERIFY(controller.requestUpload(localFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(secondServerSocket->bytesAvailable() > 0);

        FrameParser retryUploadStartParser;
        retryUploadStartParser.appendData(secondServerSocket->readAll());
        const FrameParser::FrameParseResult retryUploadStart = retryUploadStartParser.tryTakeFrame();
        QCOMPARE(retryUploadStart.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(retryUploadStart.frame.header.messageType, MessageType::UploadStartRequest);
        QVERIFY(retryUploadStart.frame.header.requestId != uploadRequestId);
    }

    void uploadFile_whileAnotherUploadIsPending_failsLocallyWithoutSendingSecondRequest()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString firstLocalFilePath = localDirectory.filePath(QStringLiteral("first.bin"));
        const QString secondLocalFilePath = localDirectory.filePath(QStringLiteral("second.bin"));
        QFile firstLocalFile(firstLocalFilePath);
        QVERIFY(firstLocalFile.open(QIODevice::WriteOnly));
        QCOMPARE(firstLocalFile.write(QByteArrayLiteral("first")), qint64{5});
        firstLocalFile.close();
        QFile secondLocalFile(secondLocalFilePath);
        QVERIFY(secondLocalFile.open(QIODevice::WriteOnly));
        QCOMPARE(secondLocalFile.write(QByteArrayLiteral("second")), qint64{6});
        secondLocalFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(authenticationResponse.encodedFrame), static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());

        QVERIFY(controller.requestUpload(firstLocalFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser firstUploadParser;
        firstUploadParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult firstUploadRequest = firstUploadParser.tryTakeFrame();
        QCOMPARE(firstUploadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstUploadRequest.frame.header.messageType, MessageType::UploadStartRequest);
        const RequestId firstUploadRequestId = firstUploadRequest.frame.header.requestId;

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());
        QVERIFY(!controller.requestUpload(secondLocalFilePath, QStringLiteral("/Documents")));
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("An upload is already in progress."));
        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});

        const ErrorResponseEncodeResult errorPayload = serializeErrorResponse(
            {ErrorCode::FileNotFound, QStringLiteral("The destination directory does not exist.")});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);
        const auto errorResponse = serializeFrame(
            MessageType::ErrorResponse,
            firstUploadRequestId,
            firstUploadRequest.frame.header.taskId,
            errorPayload.payload);
        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(errorResponse.encodedFrame), static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_COMPARE(errorSpy.count(), 2);
        QCOMPARE(errorSpy.at(1).at(0).toString(), QStringLiteral("The destination directory does not exist."));
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestUpload(secondLocalFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser secondUploadParser;
        secondUploadParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult secondUploadRequest = secondUploadParser.tryTakeFrame();
        QCOMPARE(secondUploadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondUploadRequest.frame.header.messageType, MessageType::UploadStartRequest);
        QVERIFY(secondUploadRequest.frame.header.requestId != firstUploadRequestId);
        const UploadStartRequestDecodeResult decodedSecondUpload = deserializeUploadStartRequest(secondUploadRequest.frame.payload);
        QCOMPARE(decodedSecondUpload.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedSecondUpload.data.fileName, QStringLiteral("second.bin"));
    }

    void downloadFile_whileUploadIsPending_failsLocallyWithoutSendingDownloadRequest()
    {
        QTemporaryDir localDirectory;
        QVERIFY(localDirectory.isValid());
        const QString uploadFilePath = localDirectory.filePath(QStringLiteral("upload.bin"));
        const QString downloadFilePath = localDirectory.filePath(QStringLiteral("download.bin"));
        QFile uploadFile(uploadFilePath);
        QVERIFY(uploadFile.open(QIODevice::WriteOnly));
        QCOMPARE(uploadFile.write(QByteArrayLiteral("upload")), qint64{6});
        uploadFile.close();
        QVERIFY(!QFileInfo::exists(downloadFilePath));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);

        FrameParser authenticationParser;
        authenticationParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult authenticationRequest = authenticationParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);
        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(authenticationResponse.encodedFrame), static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
        QSignalSpy errorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(errorSpy.isValid());

        QVERIFY(controller.requestUpload(uploadFilePath, QStringLiteral("/Documents")));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser uploadStartParser;
        uploadStartParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult uploadStart = uploadStartParser.tryTakeFrame();
        QCOMPARE(uploadStart.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadStart.frame.header.messageType, MessageType::UploadStartRequest);
        const RequestId uploadRequestId = uploadStart.frame.header.requestId;

        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());
        QVERIFY(!controller.requestDownload(QStringLiteral("/Documents/report.bin"), downloadFilePath));
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("An upload is already in progress."));
        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QVERIFY(!QFileInfo::exists(downloadFilePath));

        const ErrorResponseEncodeResult errorPayload = serializeErrorResponse(
            {ErrorCode::FileNotFound, QStringLiteral("The destination directory does not exist.")});
        QCOMPARE(errorPayload.status, ErrorResponseEncodeResult::Status::Success);
        const auto errorResponse = serializeFrame(
            MessageType::ErrorResponse,
            uploadRequestId,
            uploadStart.frame.header.taskId,
            errorPayload.payload);
        QCOMPARE(errorResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(serverSocket->write(errorResponse.encodedFrame), static_cast<qint64>(errorResponse.encodedFrame.size()));

        QTRY_COMPARE(errorSpy.count(), 2);
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QVERIFY(controller.requestDownload(QStringLiteral("/Documents/report.bin"), downloadFilePath));
        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(serverSocket->readAll());
        const FrameParser::FrameParseResult downloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(downloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(downloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        QVERIFY(downloadRequest.frame.header.requestId != uploadRequestId);

        const DownloadRequestDecodeResult decodedDownload = deserializeDownloadRequest(downloadRequest.frame.payload);
        QCOMPARE(decodedDownload.status, DownloadRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDownload.data.path, QStringLiteral("/Documents/report.bin"));
    }

    void createDirectory_blankRequiredField_failsLocallyWithoutSendingFrame_data()
    {
        QTest::addColumn<QString>("parentPath");
        QTest::addColumn<QString>("name");

        QTest::newRow("blank-parent-path") << QString() << QStringLiteral("Documents");
        QTest::newRow("whitespace-parent-path") << QStringLiteral("   ") << QStringLiteral("Documents");
        QTest::newRow("blank-directory-name") << QStringLiteral("/") << QString();
        QTest::newRow("whitespace-directory-name") << QStringLiteral("/") << QStringLiteral("   ");
    }

    void createDirectory_blankRequiredField_failsLocallyWithoutSendingFrame()
    {
        QFETCH(QString, parentPath);
        QFETCH(QString, name);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        ApplicationController controller;
        QVERIFY(controller.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_VERIFY(controller.isConnected());
        QTRY_VERIFY(server.hasPendingConnections());

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);

        const RequestSendResult activationResult = controller.activate(
            QStringLiteral("MCLD-1111-2222-3333-4444"), QStringLiteral("DEVICE-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);

        QTRY_VERIFY(serverSocket->bytesAvailable() > 0);
        FrameParser authenticationRequestParser;
        authenticationRequestParser.appendData(serverSocket->readAll());

        const FrameParser::FrameParseResult authenticationRequest = authenticationRequestParser.tryTakeFrame();
        QCOMPARE(authenticationRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationRequest.frame.header.messageType, MessageType::AuthenticateRequest);

        const AuthenticationEncodeResult authenticationPayload = serializeAuthenticateResponse({AuthenticationStatus::Valid});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        const auto authenticationResponse = serializeFrame(
            MessageType::AuthenticateResponse,
            activationResult.requestId,
            TaskId{0},
            authenticationPayload.payload);
        QCOMPARE(authenticationResponse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            serverSocket->write(authenticationResponse.encodedFrame),
            static_cast<qint64>(authenticationResponse.encodedFrame.size()));

        QTRY_COMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());

        QSignalSpy validationErrorSpy(&controller, &ApplicationController::fileOperationFailed);
        QVERIFY(validationErrorSpy.isValid());
        QSignalSpy peerReadyReadSpy(serverSocket, &QTcpSocket::readyRead);
        QVERIFY(peerReadyReadSpy.isValid());

        QVERIFY(!controller.requestCreateDirectory(parentPath, name));

        QCOMPARE(validationErrorSpy.count(), 1);
        QVERIFY(!validationErrorSpy.at(0).at(0).toString().isEmpty());

        QTest::qWait(100);
        QCOMPARE(peerReadyReadSpy.count(), 0);
        QCOMPARE(serverSocket->bytesAvailable(), qint64{0});
        QCOMPARE(controller.accessState(), ClientAccessState::Active);
        QVERIFY(controller.isFeatureAccessAllowed());
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
