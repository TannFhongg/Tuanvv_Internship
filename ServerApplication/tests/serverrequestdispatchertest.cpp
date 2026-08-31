#include <QtTest/QtTest>
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>

#include "ClientSession.h"
#include "ServerRequestDispatcher.h"
#include "authentication.h"
#include "errorresponse.h"
#include "frameparser.h"
#include "licensemanager.h"
#include "licenserecord.h"
#include "licenserepository.h"
#include "protocolframe.h"
#include "protocoltypes.h"

using MiniCloud::Protocol::AuthenticateRequestData;
using MiniCloud::Protocol::AuthenticateResponseDecodeResult;
using MiniCloud::Protocol::AuthenticationEncodeResult;
using MiniCloud::Protocol::AuthenticationStatus;
using MiniCloud::Protocol::ErrorCode;
using MiniCloud::Protocol::ErrorResponseDecodeResult;
using MiniCloud::Protocol::FrameParser;
using MiniCloud::Protocol::MessageType;
using MiniCloud::Protocol::ProtocolFrame;
using MiniCloud::Protocol::RequestId;
using MiniCloud::Protocol::TaskId;
using MiniCloud::Server::LicenseManager;
using MiniCloud::Server::LicenseManagerOperationStatus;
using MiniCloud::Server::LicenseManagerResult;
using MiniCloud::Server::LicenseRecord;
using MiniCloud::Server::LicenseRepository;
using MiniCloud::Server::LicenseRepositoryResult;
using MiniCloud::Server::LicenseRepositoryStatus;

class ServerRequestDispatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void authenticateRequest_validBoundLicense_returnsCorrelatedValidResponseAndAuthenticatesSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(licenseFilePath);
        const LicenseRecord license{
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OWNER"),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        const AuthenticateRequestData requestData{license.productKey, license.deviceId};
        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(responseFrame.payload);
        QCOMPARE(decoded.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());
    }

    void authenticateRequest_unknownProductKey_returnsInvalidKeyWithoutAuthenticatingSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        const AuthenticateRequestData requestData{
            QStringLiteral("MCLD-FFFF-FFFF-FFFF-FFFF"),
            QStringLiteral("DEVICE-CLIENT")};

        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(responseFrame.payload);
        QCOMPARE(decoded.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.status, AuthenticationStatus::InvalidKey);

        QVERIFY(!session.isAuthenticated());
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QVERIFY(!QFileInfo::exists(licenseFilePath));
    }

    void authenticateRequest_disabledLicense_returnsDisabledBeforeDeviceCheckWithoutAuthenticatingSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const LicenseRecord disabledRecord{
            productKey,
            QStringLiteral("DEVICE-OWNER"),
            false};

        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(disabledRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        const AuthenticateRequestData requestData{
            productKey,
            QStringLiteral("DEVICE-INTRUDER")};

        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};

        QVERIFY(!session.isAuthenticated());

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(responseFrame.payload);
        QCOMPARE(decoded.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.status, AuthenticationStatus::Disabled);

        QVERIFY(!session.isAuthenticated());
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
    }

    void authenticateRequest_boundLicenseWithDifferentDevice_returnsDeviceMismatchWithoutAuthenticatingSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const LicenseRecord boundRecord{
            productKey,
            QStringLiteral("DEVICE-OWNER"),
            true};

        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(boundRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        const AuthenticateRequestData requestData{
            productKey,
            QStringLiteral("DEVICE-INTRUDER")};

        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};

        QVERIFY(!session.isAuthenticated());

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(responseFrame.payload);
        QCOMPARE(decoded.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.status, AuthenticationStatus::DeviceMismatch);

        QVERIFY(!session.isAuthenticated());
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        LicenseRepository verificationRepository(licenseFilePath);
        const LicenseRepositoryResult reloadResult = verificationRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);

        const auto storedRecord = verificationRepository.findByProductKey(productKey);
        QVERIFY(storedRecord.has_value());
        QCOMPARE(storedRecord->deviceId, QStringLiteral("DEVICE-OWNER"));
        QVERIFY(storedRecord->enabled);
    }

    void authenticateRequest_unboundLicense_bindsDeviceReturnsValidAndAuthenticatesSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const LicenseRecord unboundRecord{
            productKey,
            QString(),
            true};

        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const AuthenticateRequestData requestData{
            productKey,
            deviceId};

        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};

        QVERIFY(!session.isAuthenticated());

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(responseFrame.payload);
        QCOMPARE(decoded.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.status, AuthenticationStatus::Valid);

        QVERIFY(session.isAuthenticated());
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        LicenseRepository verificationRepository(licenseFilePath);

        const auto reloadResult = verificationRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);

        const auto persistedRecord = verificationRepository.findByProductKey(productKey);

        QVERIFY(persistedRecord.has_value());
        QCOMPARE(persistedRecord->productKey, productKey);
        QCOMPARE(persistedRecord->deviceId, deviceId);
        QVERIFY(persistedRecord->enabled);
    }

    void authenticateRequest_unboundLicensePersistenceFails_returnsCorrelatedInternalServerErrorWithoutAuthenticatingSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storagePath = temporaryDirectory.filePath(QStringLiteral("storage"));

        const QString movedStoragePath = temporaryDirectory.filePath(QStringLiteral("storage-moved"));

        const QString repositoryFilePath = QDir(storagePath).filePath(QStringLiteral("licenses.json"));

        const QString movedRepositoryFilePath = QDir(movedStoragePath).filePath(QStringLiteral("licenses.json"));

        QDir temporaryDirectoryPath(temporaryDirectory.path());
        QVERIFY(temporaryDirectoryPath.mkpath(QStringLiteral("storage")));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        const QString productKey = QStringLiteral("MCLD-1111-2222-3333-4444");
        const LicenseRecord unboundRecord{
            productKey,
            QString(),
            true};

        LicenseRepository repository(repositoryFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(repositoryFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(QFileInfo::exists(movedRepositoryFilePath));

        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const AuthenticateRequestData requestData{
            productKey,
            deviceId};

        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};

        QVERIFY(!session.isAuthenticated());

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);
        QVERIFY(!session.isAuthenticated());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::InternalServerError);
        QCOMPARE(decoded.data.message, QStringLiteral("Unable to process the authentication request."));

        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        LicenseRepository verificationRepository(movedRepositoryFilePath);

        QCOMPARE(verificationRepository.load().status, LicenseRepositoryStatus::Success);

        const auto record = verificationRepository.findByProductKey(productKey);

        QVERIFY(record.has_value());
        QVERIFY(record->deviceId.isEmpty());
        QVERIFY(record->enabled);
    }

    void authenticateRequest_malformedPayload_returnsCorrelatedInvalidRequestWithoutAuthenticatingSession()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        QVERIFY(!QFileInfo::exists(repositoryFilePath));

        LicenseManager licenseManager(repositoryFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        ProtocolFrame requestFrame;
        requestFrame.header.messageType = MessageType::AuthenticateRequest;
        requestFrame.header.requestId = 47;
        requestFrame.header.taskId = 0;
        requestFrame.payload = QByteArrayLiteral("{not-valid-json");
        requestFrame.header.payloadLength = static_cast<quint32>(requestFrame.payload.size());

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, requestFrame.header.requestId);
        QCOMPARE(responseFrame.header.taskId, requestFrame.header.taskId);

        const ErrorResponseDecodeResult decodedError = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decodedError.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decodedError.data.errorCode, ErrorCode::InvalidRequest);
        QVERIFY(!decodedError.data.message.isEmpty());

        QVERIFY(!session.isAuthenticated());
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QVERIFY(!QFileInfo::exists(repositoryFilePath));
    }

    void fileChunk_beforeAuthentication_returnsCorrelatedAuthenticationFailedAndKeepsSessionLocked()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseManager licenseManager(repositoryFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        ClientSession session(serverSocket);
        QVERIFY(!session.isAuthenticated());

        ProtocolFrame requestFrame;
        requestFrame.header.messageType = MessageType::FileChunk;
        requestFrame.header.requestId = 48;
        requestFrame.header.taskId = 9001;
        requestFrame.payload = QByteArrayLiteral("protected-content");
        requestFrame.header.payloadLength = static_cast<quint32>(requestFrame.payload.size());

        ServerRequestDispatcher dispatcher(licenseManager);
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, requestFrame.header.requestId);
        QCOMPARE(responseFrame.header.taskId, requestFrame.header.taskId);

        const ErrorResponseDecodeResult decodedError = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decodedError.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decodedError.data.errorCode, ErrorCode::AuthenticationFailed);
        QVERIFY(!decodedError.data.message.isEmpty());

        QVERIFY(!session.isAuthenticated());
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
    }

    void reconnect_afterAuthenticatedSession_startsLockedAndRequiresAuthenticationAgain()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        const LicenseRecord license{
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OWNER"),
            true};

        LicenseRepository repository(repositoryFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(repositoryFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);
        ServerRequestDispatcher dispatcher(licenseManager);

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientA;
        clientA.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocketA = server.nextPendingConnection();
        QVERIFY(serverSocketA != nullptr);
        QTRY_COMPARE(serverSocketA->state(), QAbstractSocket::ConnectedState);

        ClientSession sessionA(serverSocketA);
        QVERIFY(!sessionA.isAuthenticated());

        const AuthenticateRequestData authenticationData{license.productKey, license.deviceId};
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest(authenticationData);
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        ProtocolFrame authenticationFrame;
        authenticationFrame.header.messageType = MessageType::AuthenticateRequest;
        authenticationFrame.header.requestId = 49;
        authenticationFrame.header.taskId = 0;
        authenticationFrame.payload = authenticationPayload.payload;
        authenticationFrame.header.payloadLength =
            static_cast<quint32>(authenticationFrame.payload.size());

        dispatcher.handleFrame(sessionA, authenticationFrame);

        QTRY_VERIFY(clientA.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientA.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status,AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(sessionA.isAuthenticated());

        QSignalSpy finishedSpy(&sessionA, &ClientSession::sessionFinished);
        QVERIFY(finishedSpy.isValid());

        clientA.disconnectFromHost();
        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(finishedSpy.count(), 1);
        QVERIFY(!sessionA.isAuthenticated());

        QTcpSocket clientB;
        clientB.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientB.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocketB = server.nextPendingConnection();
        QVERIFY(serverSocketB != nullptr);
        QTRY_COMPARE(serverSocketB->state(), QAbstractSocket::ConnectedState);

        ClientSession sessionB(serverSocketB);
        QVERIFY(!sessionB.isAuthenticated());

        ProtocolFrame fileChunkFrame;
        fileChunkFrame.header.messageType = MessageType::FileChunk;
        fileChunkFrame.header.requestId = 48;
        fileChunkFrame.header.taskId = 9001;
        fileChunkFrame.payload = QByteArrayLiteral("protected-content");
        fileChunkFrame.header.payloadLength = static_cast<quint32>(fileChunkFrame.payload.size());

        dispatcher.handleFrame(sessionB, fileChunkFrame);

        QTRY_VERIFY(clientB.bytesAvailable() > 0);
        FrameParser fileChunkResponseParser;
        fileChunkResponseParser.appendData(clientB.readAll());
        const FrameParser::FrameParseResult fileChunkResponse = fileChunkResponseParser.tryTakeFrame();

        QCOMPARE(fileChunkResponse.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = fileChunkResponse.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);

        const ErrorResponseDecodeResult decodedError = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decodedError.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decodedError.data.errorCode, ErrorCode::AuthenticationFailed);
        QCOMPARE(responseFrame.header.requestId, fileChunkFrame.header.requestId);
        QCOMPARE(responseFrame.header.taskId, fileChunkFrame.header.taskId);

        QVERIFY(!sessionB.isAuthenticated());
        QCOMPARE(clientB.state(), QAbstractSocket::ConnectedState);
    }
};

QTEST_GUILESS_MAIN(ServerRequestDispatcherTest)
#include "serverrequestdispatchertest.moc"
