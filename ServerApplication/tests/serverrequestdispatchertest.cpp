#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>

#include "ClientSession.h"
#include "ServerRequestDispatcher.h"
#include "authentication.h"
#include "errorresponse.h"
#include "filemanager.h"
#include "fileprotocol.h"
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
using MiniCloud::Protocol::BrowseResponseDecodeResult;
using MiniCloud::Protocol::ErrorCode;
using MiniCloud::Protocol::ErrorResponseDecodeResult;
using MiniCloud::Protocol::FileEntryType;
using MiniCloud::Protocol::FrameParser;
using MiniCloud::Protocol::MessageType;
using MiniCloud::Protocol::ProtocolFrame;
using MiniCloud::Protocol::RequestId;
using MiniCloud::Protocol::TaskId;
using MiniCloud::Protocol::UploadReadyResponseDecodeResult;
using MiniCloud::Server::FileManager;
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
        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};

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

        const AuthenticateRequestData requestData{QStringLiteral("MCLD-FFFF-FFFF-FFFF-FFFF"), QStringLiteral("DEVICE-CLIENT")};

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
        const LicenseRecord disabledRecord{productKey, QStringLiteral("DEVICE-OWNER"), false};

        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(disabledRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        const AuthenticateRequestData requestData{productKey, QStringLiteral("DEVICE-INTRUDER")};

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
        const LicenseRecord boundRecord{productKey, QStringLiteral("DEVICE-OWNER"), true};

        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(boundRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        const AuthenticateRequestData requestData{productKey, QStringLiteral("DEVICE-INTRUDER")};

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
        const LicenseRecord unboundRecord{productKey, QString(), true};

        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        const QString deviceId = QStringLiteral("DEVICE-CLIENT");
        const AuthenticateRequestData requestData{productKey, deviceId};

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
        const LicenseRecord unboundRecord{productKey, QString(), true};

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
        const AuthenticateRequestData requestData{productKey, deviceId};

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

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};

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
        authenticationFrame.header.payloadLength = static_cast<quint32>(authenticationFrame.payload.size());

        dispatcher.handleFrame(sessionA, authenticationFrame);

        QTRY_VERIFY(clientA.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientA.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
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

    void fileChunk_withoutActiveUpload_returnsCorrelatedInvalidRequestAndCreatesNoFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             40,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse =
            authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult chunkPayload =
            MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("data")});
        QCOMPARE(chunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 45;
        const ProtocolFrame chunkFrame{
            {0,
             0,
             MessageType::FileChunk,
             static_cast<quint32>(chunkPayload.payload.size()),
             requestId,
             taskId},
            chunkPayload.payload};
        dispatcher.handleFrame(session, chunkFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded =
            MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::InvalidRequest);
        QVERIFY(!decoded.data.message.isEmpty());

        const MiniCloud::Server::FileManagerBrowseResult browseResult =
            fileManager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, MiniCloud::Server::FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
        QVERIFY(session.isAuthenticated());
    }

    void uploadStartRequest_malformedPayload_returnsCorrelatedInvalidRequestWithoutCreatingFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             40,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse =
            authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QVERIFY(session.isAuthenticated());

        const QByteArray malformedPayload =
            QByteArrayLiteral(R"({"destinationDirectoryPath":})");
        constexpr RequestId requestId = 44;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(malformedPayload.size()),
             requestId,
             taskId},
            malformedPayload};
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded =
            MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::InvalidRequest);
        QVERIFY(!decoded.data.message.isEmpty());

        const QString finalNativePath =
            QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        QVERIFY(!QFileInfo::exists(finalNativePath));
        const MiniCloud::Server::FileManagerBrowseResult browseResult =
            fileManager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, MiniCloud::Server::FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
        QVERIFY(session.isAuthenticated());
    }

    void uploadStartRequest_beforeAuthentication_returnsCorrelatedAuthenticationFailedAndCreatesNoFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        LicenseManager licenseManager(licenseFilePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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

        const MiniCloud::Protocol::FileProtocolEncodeResult requestPayload = MiniCloud::Protocol::serializeUploadStartRequest(
            {QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5});
        QCOMPARE(requestPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 43;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};
        dispatcher.handleFrame(session, requestFrame);

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
        QCOMPARE(decoded.data.errorCode, ErrorCode::AuthenticationFailed);
        QVERIFY(!decoded.data.message.isEmpty());

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        QVERIFY(!QFileInfo::exists(finalNativePath));
        const MiniCloud::Server::FileManagerBrowseResult browseResult = fileManager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, MiniCloud::Server::FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
        QVERIFY(!session.isAuthenticated());
    }

    void uploadStartRequest_zeroByteFile_commitsImmediatelyAndReturnsCorrelatedOperationResponse()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             40,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult zeroBytePayload = MiniCloud::Protocol::serializeUploadStartRequest(
            {QStringLiteral("/Documents"), QStringLiteral("empty.bin"), 0});
        QCOMPARE(zeroBytePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId zeroByteRequestId = 41;
        const ProtocolFrame zeroByteRequestFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(zeroBytePayload.payload.size()),
             zeroByteRequestId,
             taskId},
            zeroBytePayload.payload};
        dispatcher.handleFrame(session, zeroByteRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser zeroByteResponseParser;
        zeroByteResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult zeroByteResponse = zeroByteResponseParser.tryTakeFrame();
        QCOMPARE(zeroByteResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(zeroByteResponse.frame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(zeroByteResponse.frame.header.requestId, zeroByteRequestId);
        QCOMPARE(zeroByteResponse.frame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(zeroByteResponse.frame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents/empty.bin"));

        const QString emptyFilePath = QDir(storageRoot).filePath(QStringLiteral("Documents/empty.bin"));
        QVERIFY(QFileInfo(emptyFilePath).isFile());
        QCOMPARE(QFileInfo(emptyFilePath).size(), qint64{0});

        const MiniCloud::Protocol::FileProtocolEncodeResult nonZeroPayload =
            MiniCloud::Protocol::serializeUploadStartRequest({QStringLiteral("/Documents"), QStringLiteral("next.bin"), 1});
        QCOMPARE(nonZeroPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId nonZeroRequestId = 42;
        const ProtocolFrame nonZeroRequestFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(nonZeroPayload.payload.size()),
             nonZeroRequestId,
             taskId},
            nonZeroPayload.payload};
        dispatcher.handleFrame(session, nonZeroRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser nonZeroResponseParser;
        nonZeroResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult nonZeroResponse = nonZeroResponseParser.tryTakeFrame();
        QCOMPARE(nonZeroResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(nonZeroResponse.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(nonZeroResponse.frame.header.requestId, nonZeroRequestId);
        QCOMPARE(nonZeroResponse.frame.header.taskId, taskId);
        QVERIFY(session.isAuthenticated());
    }

    void fileChunk_withMismatchedRequestId_returnsCorrelatedInvalidRequestWithoutAffectingUpload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             40,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult uploadStartPayload = MiniCloud::Protocol::serializeUploadStartRequest(
            {QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5});
        QCOMPARE(uploadStartPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId uploadRequestId = 41;
        const ProtocolFrame uploadStartFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(uploadStartPayload.payload.size()),
             uploadRequestId,
             taskId},
            uploadStartPayload.payload};
        dispatcher.handleFrame(session, uploadStartFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser uploadReadyParser;
        uploadReadyParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult uploadReadyResponse = uploadReadyParser.tryTakeFrame();
        QCOMPARE(uploadReadyResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadReadyResponse.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(uploadReadyResponse.frame.header.requestId, uploadRequestId);

        const MiniCloud::Protocol::FileProtocolEncodeResult mismatchedChunkPayload = MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("abc")});
        QCOMPARE(mismatchedChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId mismatchedRequestId = 42;
        const ProtocolFrame mismatchedChunkFrame{
            {0,
             0,
             MessageType::FileChunk,
             static_cast<quint32>(mismatchedChunkPayload.payload.size()),
             mismatchedRequestId,
             taskId},
            mismatchedChunkPayload.payload};
        dispatcher.handleFrame(session, mismatchedChunkFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser errorResponseParser;
        errorResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult errorResponse = errorResponseParser.tryTakeFrame();
        QCOMPARE(errorResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(errorResponse.frame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(errorResponse.frame.header.requestId, mismatchedRequestId);
        QCOMPARE(errorResponse.frame.header.taskId, taskId);

        const ErrorResponseDecodeResult decodedError = MiniCloud::Protocol::deserializeErrorResponse(errorResponse.frame.payload);
        QCOMPARE(decodedError.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decodedError.data.errorCode, ErrorCode::InvalidRequest);
        QVERIFY(!decodedError.data.message.isEmpty());

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        QVERIFY(!QFileInfo::exists(finalNativePath));
        const MiniCloud::Server::FileManagerBrowseResult browseResult = fileManager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, MiniCloud::Server::FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());

        const MiniCloud::Protocol::FileProtocolEncodeResult validChunkPayload = MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("abcde")});
        QCOMPARE(validChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const ProtocolFrame validChunkFrame{
            {0,
             0,
             MessageType::FileChunk,
             static_cast<quint32>(validChunkPayload.payload.size()),
             uploadRequestId,
             taskId},
            validChunkPayload.payload};
        dispatcher.handleFrame(session, validChunkFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser completionResponseParser;
        completionResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult completionResponse = completionResponseParser.tryTakeFrame();
        QCOMPARE(completionResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(completionResponse.frame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(completionResponse.frame.header.requestId, uploadRequestId);
        QCOMPARE(completionResponse.frame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decodedCompletion = MiniCloud::Protocol::deserializeFileOperationResponse(completionResponse.frame.payload);
        QCOMPARE(decodedCompletion.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decodedCompletion.data.path, QStringLiteral("/Documents/report.bin"));

        QFile finalFile(finalNativePath);
        QVERIFY(finalFile.open(QIODevice::ReadOnly));
        QCOMPARE(finalFile.readAll(), QByteArrayLiteral("abcde"));
        QVERIFY(session.isAuthenticated());
    }

    void fileChunk_afterUploadReady_commitsFileAndReturnsCorrelatedOperationResponse()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});

        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             40,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult uploadStartPayload = MiniCloud::Protocol::serializeUploadStartRequest(
            {QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5});
        QCOMPARE(uploadStartPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId uploadRequestId = 41;
        const ProtocolFrame uploadStartFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(uploadStartPayload.payload.size()),
             uploadRequestId,
             taskId},
            uploadStartPayload.payload};
        dispatcher.handleFrame(session, uploadStartFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser uploadReadyParser;
        uploadReadyParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult uploadReadyResponse = uploadReadyParser.tryTakeFrame();
        QCOMPARE(uploadReadyResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadReadyResponse.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(uploadReadyResponse.frame.header.requestId, uploadRequestId);

        const UploadReadyResponseDecodeResult decodedReady =
            MiniCloud::Protocol::deserializeUploadReadyResponse(uploadReadyResponse.frame.payload);
        QCOMPARE(decodedReady.status, UploadReadyResponseDecodeResult::Status::Success);
        QCOMPARE(decodedReady.data.path, QStringLiteral("/Documents/report.bin"));

        const MiniCloud::Protocol::FileProtocolEncodeResult firstChunkPayload =
            MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("abc")});
        QCOMPARE(firstChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const ProtocolFrame firstChunkFrame{
            {0,
             0,
             MessageType::FileChunk,
             static_cast<quint32>(firstChunkPayload.payload.size()),
             uploadRequestId,
             taskId},
            firstChunkPayload.payload};
        dispatcher.handleFrame(session, firstChunkFrame);

        QCOMPARE(clientSocket.bytesAvailable(), qint64{0});

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));

        QVERIFY(!QFileInfo::exists(finalNativePath));

        const MiniCloud::Server::FileManagerBrowseResult interimBrowseResult = fileManager.browse(QStringLiteral("/Documents"));
        QCOMPARE(interimBrowseResult.status, MiniCloud::Server::FileManagerOperationStatus::Success);
        QVERIFY(interimBrowseResult.entries.isEmpty());

        const MiniCloud::Protocol::FileProtocolEncodeResult finalChunkPayload = MiniCloud::Protocol::serializeFileChunk({3, QByteArrayLiteral("de")});
        QCOMPARE(finalChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const ProtocolFrame finalChunkFrame{
            {0,
             0,
             MessageType::FileChunk,
             static_cast<quint32>(finalChunkPayload.payload.size()),
             uploadRequestId,
             taskId},
            finalChunkPayload.payload};
        dispatcher.handleFrame(session, finalChunkFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(responseFrame.header.requestId, uploadRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(responseFrame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents/report.bin"));

        QFile finalFile(finalNativePath);
        QVERIFY(finalFile.open(QIODevice::ReadOnly));
        QCOMPARE(finalFile.readAll(), QByteArrayLiteral("abcde"));
        QVERIFY(session.isAuthenticated());
    }

    void uploadStartRequest_authenticatedSession_returnsCorrelatedReadyResponseWithoutCreatingFinalFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 600;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);

        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());

        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult requestPayload = MiniCloud::Protocol::serializeUploadStartRequest(
            {QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5});

        QCOMPARE(requestPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId uploadRequestId = 601;
        const ProtocolFrame uploadRequestFrame{
            {0,
             0,
             MessageType::UploadStartRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             uploadRequestId,
             taskId},
            requestPayload.payload};
        dispatcher.handleFrame(session, uploadRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(responseFrame.header.requestId, uploadRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const UploadReadyResponseDecodeResult decoded = MiniCloud::Protocol::deserializeUploadReadyResponse(responseFrame.payload);
        QCOMPARE(decoded.status, UploadReadyResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents/report.bin"));

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const MiniCloud::Server::FileManagerBrowseResult browseResult = fileManager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, MiniCloud::Server::FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
        QVERIFY(session.isAuthenticated());
    }

    void browseRequest_authenticatedSession_returnsCorrelatedEntries()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Reports"))));

        const QString alphaPath = QDir(storageRoot).filePath(QStringLiteral("Documents/alpha.txt"));
        QFile alphaFile(alphaPath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("alpha"), qint64{5});
        alphaFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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

        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 201;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult browsePayload = MiniCloud::Protocol::serializeBrowseRequest({QStringLiteral("/Documents")});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId browseRequestId = 202;
        const ProtocolFrame browseRequestFrame{
            {0,
             0,
             MessageType::BrowseRequest,
             static_cast<quint32>(browsePayload.payload.size()),
             browseRequestId,
             taskId},
            browsePayload.payload};
        dispatcher.handleFrame(session, browseRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser browseResponseParser;
        browseResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult browseResponse = browseResponseParser.tryTakeFrame();
        QCOMPARE(browseResponse.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = browseResponse.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::BrowseResponse);
        QCOMPARE(responseFrame.header.requestId, browseRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const BrowseResponseDecodeResult decoded = MiniCloud::Protocol::deserializeBrowseResponse(responseFrame.payload);
        QCOMPARE(decoded.status, BrowseResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents"));
        QCOMPARE(decoded.data.entries.size(), 2);

        const auto &reports = decoded.data.entries.at(0);
        QCOMPARE(reports.name, QStringLiteral("Reports"));
        QCOMPARE(reports.type, FileEntryType::Directory);
        QCOMPARE(reports.path, QStringLiteral("/Documents/Reports"));

        const auto &alpha = decoded.data.entries.at(1);
        QCOMPARE(alpha.name, QStringLiteral("alpha.txt"));
        QCOMPARE(alpha.type, FileEntryType::File);
        QCOMPARE(alpha.path, QStringLiteral("/Documents/alpha.txt"));
    }

    void browseRequest_beforeAuthentication_returnsCorrelatedAuthenticationFailedAndKeepsSessionLocked()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        const QString fixturePath = QDir(storageRoot).filePath(QStringLiteral("fixture.txt"));
        QFile fixtureFile(fixturePath);
        QVERIFY(fixtureFile.open(QIODevice::WriteOnly));
        QCOMPARE(fixtureFile.write("fixture"), qint64{7});
        fixtureFile.close();

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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

        const MiniCloud::Protocol::FileProtocolEncodeResult browsePayload = MiniCloud::Protocol::serializeBrowseRequest({QStringLiteral("/")});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId browseRequestId = 203;
        constexpr TaskId taskId = 0;
        const ProtocolFrame browseRequestFrame{
            {0,
             0,
             MessageType::BrowseRequest,
             static_cast<quint32>(browsePayload.payload.size()),
             browseRequestId,
             taskId},
            browsePayload.payload};

        dispatcher.handleFrame(session, browseRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, browseRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::AuthenticationFailed);
        QVERIFY(!decoded.data.message.isEmpty());

        QVERIFY(!session.isAuthenticated());
        QVERIFY(QFileInfo(fixturePath).isFile());
    }

    void browseRequest_malformedPayload_returnsCorrelatedInvalidRequestWithoutMutatingStorage()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString alphaPath = QDir(storageRoot).filePath(QStringLiteral("Documents/alpha.txt"));
        QFile alphaFile(alphaPath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("alpha"), qint64{5});
        alphaFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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

        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 204;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const QByteArray malformedPayload = QByteArrayLiteral(R"({"path":})");
        constexpr RequestId browseRequestId = 205;
        const ProtocolFrame malformedBrowseFrame{
            {0,
             0,
             MessageType::BrowseRequest,
             static_cast<quint32>(malformedPayload.size()),
             browseRequestId,
             taskId},
            malformedPayload};
        dispatcher.handleFrame(session, malformedBrowseFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, browseRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::InvalidRequest);
        QVERIFY(!decoded.data.message.isEmpty());

        QVERIFY(session.isAuthenticated());
        QVERIFY(QFileInfo(alphaPath).isFile());
    }

    void browseRequest_missingDirectory_returnsCorrelatedFileNotFoundAndKeepsSessionAuthenticated()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString documentsNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents"));
        QVERIFY(QDir().mkpath(documentsNativePath));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 206;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult browsePayload = MiniCloud::Protocol::serializeBrowseRequest({QStringLiteral("/Missing")});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId browseRequestId = 207;
        const ProtocolFrame browseRequestFrame{
            {0,
             0,
             MessageType::BrowseRequest,
             static_cast<quint32>(browsePayload.payload.size()),
             browseRequestId,
             taskId},
            browsePayload.payload};

        dispatcher.handleFrame(session, browseRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, browseRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::FileNotFound);
        QVERIFY(!decoded.data.message.isEmpty());

        QVERIFY(session.isAuthenticated());
        QVERIFY(QFileInfo::exists(documentsNativePath));
    }

    void browseRequest_nonCanonicalPath_returnsCorrelatedInvalidRequestAndKeepsSessionAuthenticated()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString alphaNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/alpha.txt"));
        QFile alphaFile(alphaNativePath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("alpha"), qint64{5});
        alphaFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 208;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult browsePayload = MiniCloud::Protocol::serializeBrowseRequest({QStringLiteral("/../outside")});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId browseRequestId = 209;
        const ProtocolFrame browseRequestFrame{
            {0,
             0,
             MessageType::BrowseRequest,
             static_cast<quint32>(browsePayload.payload.size()),
             browseRequestId,
             taskId},
            browsePayload.payload};
        dispatcher.handleFrame(session, browseRequestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::ErrorResponse);
        QCOMPARE(responseFrame.header.requestId, browseRequestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const ErrorResponseDecodeResult decoded = MiniCloud::Protocol::deserializeErrorResponse(responseFrame.payload);
        QCOMPARE(decoded.status, ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.errorCode, ErrorCode::InvalidRequest);
        QVERIFY(!decoded.data.message.isEmpty());

        QVERIFY(session.isAuthenticated());
        QVERIFY(QFileInfo::exists(alphaNativePath));
    }

    void createDirectoryRequest_authenticatedSession_createsDirectoryAndReturnsCorrelatedPath()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 300;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult requestPayload = MiniCloud::Protocol::serializeCreateDirectoryRequest({QStringLiteral("/Documents"), QStringLiteral("Projects")});
        QCOMPARE(requestPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 301;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::CreateDirectoryRequest,
             static_cast<quint32>(requestPayload.payload.size()),
             requestId,
             taskId},
            requestPayload.payload};
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(responseFrame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents/Projects"));

        QVERIFY(QFileInfo(QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"))).isDir());
        QVERIFY(session.isAuthenticated());
    }

    void searchRequest_authenticatedSession_returnsCorrelatedDirectMatches()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Archive"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        QVERIFY(documentsDirectory.mkdir(QStringLiteral("Reports")));

        const QString reportPath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile reportFile(reportPath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        const QString imagePath = documentsDirectory.filePath(QStringLiteral("image.png"));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::WriteOnly));
        QCOMPARE(imageFile.write("image"), qint64{5});
        imageFile.close();

        const QString oldReportPath = QDir(documentsDirectory.filePath(QStringLiteral("Archive"))).filePath(QStringLiteral("old-report.txt"));
        QFile oldReportFile(oldReportPath);
        QVERIFY(oldReportFile.open(QIODevice::WriteOnly));
        QCOMPARE(oldReportFile.write("old-report"), qint64{10});
        oldReportFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 400;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult payload = MiniCloud::Protocol::serializeSearchRequest({QStringLiteral("/Documents"), QStringLiteral("RePoRt")});
        QCOMPARE(payload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 401;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::SearchRequest,
             static_cast<quint32>(payload.payload.size()),
             requestId,
             taskId},
            payload.payload};
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::SearchResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const MiniCloud::Protocol::SearchResponseDecodeResult decoded = MiniCloud::Protocol::deserializeSearchResponse(responseFrame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::SearchResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents"));
        QCOMPARE(decoded.data.entries.size(), 2);

        const auto &reports = decoded.data.entries.at(0);
        QCOMPARE(reports.name, QStringLiteral("Reports"));
        QCOMPARE(reports.type, FileEntryType::Directory);
        QCOMPARE(reports.path, QStringLiteral("/Documents/Reports"));

        const auto &report = decoded.data.entries.at(1);
        QCOMPARE(report.name, QStringLiteral("report.txt"));
        QCOMPARE(report.type, FileEntryType::File);
        QCOMPARE(report.path, QStringLiteral("/Documents/report.txt"));

        for (const auto &entry : decoded.data.entries)
        {
            QVERIFY(entry.name != QStringLiteral("old-report.txt"));
        }

        QVERIFY(session.isAuthenticated());
    }

    void renameRequest_authenticatedSession_renamesEntryAndReturnsCorrelatedPath()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        const QString oldNativePath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile reportFile(oldNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("quarterly-data"), qint64{14});
        reportFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 500;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult payload = MiniCloud::Protocol::serializeRenameRequest(
            {QStringLiteral("/Documents/report.txt"), QStringLiteral("summary.txt")});
        QCOMPARE(payload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 501;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::RenameRequest,
             static_cast<quint32>(payload.payload.size()),
             requestId,
             taskId},
            payload.payload};
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(
            responseFrame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents/summary.txt"));

        const QString newNativePath = documentsDirectory.filePath(QStringLiteral("summary.txt"));
        QVERIFY(!QFileInfo::exists(oldNativePath));
        QVERIFY(QFileInfo(newNativePath).isFile());

        QFile renamedFile(newNativePath);
        QVERIFY(renamedFile.open(QIODevice::ReadOnly));
        QCOMPARE(renamedFile.readAll(), QByteArrayLiteral("quarterly-data"));
        QVERIFY(session.isAuthenticated());
    }

    void moveRequest_authenticatedSession_movesEntryAndReturnsCorrelatedPath()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Archive")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString oldNativePath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile reportFile(oldNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("quarterly-data"), qint64{14});
        reportFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest(
            {license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 600;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};
        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(
            authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult payload = MiniCloud::Protocol::serializeMoveRequest(
            {QStringLiteral("/Documents/report.txt"), QStringLiteral("/Archive")});
        QCOMPARE(payload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 601;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::MoveRequest,
             static_cast<quint32>(payload.payload.size()),
             requestId,
             taskId},
            payload.payload};
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(
            responseFrame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Archive/report.txt"));

        const QString newNativePath = storageDirectory.filePath(QStringLiteral("Archive/report.txt"));
        QVERIFY(!QFileInfo::exists(oldNativePath));
        QVERIFY(QFileInfo(newNativePath).isFile());

        QFile movedFile(newNativePath);
        QVERIFY(movedFile.open(QIODevice::ReadOnly));
        QCOMPARE(movedFile.readAll(), QByteArrayLiteral("quarterly-data"));
        QVERIFY(session.isAuthenticated());
    }

    void deleteRequest_authenticatedSession_removesFileAndReturnsCorrelatedPath()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString reportNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.txt"));
        QFile reportFile(reportNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(licenseFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest(
            {license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 700;
        constexpr TaskId taskId = 0;
        const ProtocolFrame authenticationFrame{
            {0,
             0,
             MessageType::AuthenticateRequest,
             static_cast<quint32>(authenticationPayload.payload.size()),
             authenticationRequestId,
             taskId},
            authenticationPayload.payload};

        dispatcher.handleFrame(session, authenticationFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(
            authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);
        QVERIFY(session.isAuthenticated());

        const MiniCloud::Protocol::FileProtocolEncodeResult payload = MiniCloud::Protocol::serializeDeleteRequest(
            {QStringLiteral("/Documents/report.txt")});
        QCOMPARE(payload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 701;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::DeleteRequest,
             static_cast<quint32>(payload.payload.size()),
             requestId,
             taskId},
            payload.payload};
        dispatcher.handleFrame(session, requestFrame);

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(
            responseFrame.payload);
        QCOMPARE(decoded.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents/report.txt"));

        QVERIFY(!QFileInfo::exists(reportNativePath));
        QVERIFY(session.isAuthenticated());
    }

    void deleteRequest_beforeAuthentication_returnsCorrelatedAuthenticationFailedAndPreservesFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString reportNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.txt"));
        QFile reportFile(reportNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        LicenseManager licenseManager(licenseFilePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        FileManager fileManager(storageRoot);
        ServerRequestDispatcher dispatcher(licenseManager, fileManager);

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

        const MiniCloud::Protocol::FileProtocolEncodeResult payload = MiniCloud::Protocol::serializeDeleteRequest(
            {QStringLiteral("/Documents/report.txt")});
        QCOMPARE(payload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId requestId = 702;
        constexpr TaskId taskId = 0;
        const ProtocolFrame requestFrame{
            {0,
             0,
             MessageType::DeleteRequest,
             static_cast<quint32>(payload.payload.size()),
             requestId,
             taskId},
            payload.payload};
        dispatcher.handleFrame(session, requestFrame);

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
        QCOMPARE(decoded.data.errorCode, ErrorCode::AuthenticationFailed);
        QVERIFY(!decoded.data.message.isEmpty());

        QVERIFY(!session.isAuthenticated());
        QVERIFY(QFileInfo(reportNativePath).isFile());
    }
};

QTEST_GUILESS_MAIN(ServerRequestDispatcherTest)
#include "serverrequestdispatchertest.moc"
