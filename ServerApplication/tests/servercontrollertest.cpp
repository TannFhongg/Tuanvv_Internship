#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QTemporaryDir>
#include <QTcpSocket>

#include "authentication.h"
#include "fileprotocol.h"
#include "frameparser.h"
#include "licenserecord.h"
#include "licenserepository.h"
#include "protocolcodec.h"
#include "servercontroller.h"

using MiniCloud::Protocol::AuthenticateRequestData;
using MiniCloud::Protocol::AuthenticateResponseDecodeResult;
using MiniCloud::Protocol::AuthenticationEncodeResult;
using MiniCloud::Protocol::AuthenticationStatus;
using MiniCloud::Protocol::BrowseResponseDecodeResult;
using MiniCloud::Protocol::FileEntryType;
using MiniCloud::Protocol::FrameEncodeResult;
using MiniCloud::Protocol::FrameEncodeStatus;
using MiniCloud::Protocol::FrameParser;
using MiniCloud::Protocol::MessageType;
using MiniCloud::Protocol::RequestId;
using MiniCloud::Protocol::TaskId;
using MiniCloud::Server::LicenseManagerOperationStatus;
using MiniCloud::Server::LicenseManagerResult;
using MiniCloud::Server::LicenseRecord;
using MiniCloud::Server::LicenseRepository;
using MiniCloud::Server::LicenseRepositoryResult;
using MiniCloud::Server::LicenseRepositoryStatus;

class ServerControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void validAuthentication_overTcp_returnsCorrelatedValidResponse()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(repositoryFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        ServerController controller(repositoryFilePath, storageRoot);

        const LicenseManagerResult initializeResult = controller.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QVERIFY(controller.startListening(QHostAddress::LocalHost, 0));
        QVERIFY(controller.serverPort() != 0);

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, controller.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        const AuthenticateRequestData requestData{license.productKey, license.deviceId};

        const AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId requestId = 51;
        constexpr TaskId taskId = 0;
        const FrameEncodeResult encodedRequest =
            MiniCloud::Protocol::serializeFrame(
                MessageType::AuthenticateRequest,
                requestId,
                taskId,
                requestPayload.payload);
        QCOMPARE(encodedRequest.status, FrameEncodeStatus::Success);

        QCOMPARE(clientSocket.write(encodedRequest.encodedFrame), static_cast<qint64>(encodedRequest.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser responseParser;
        responseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult response = responseParser.tryTakeFrame();
        QCOMPARE(response.status, FrameParser::FrameParseStatus::FrameReady);

        const MiniCloud::Protocol::ProtocolFrame &responseFrame = response.frame;
        QCOMPARE(responseFrame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestId);
        QCOMPARE(responseFrame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(responseFrame.payload);
        QCOMPARE(decoded.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.status, AuthenticationStatus::Valid);
        QCOMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        controller.stop();
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);
    }

    void browseRequest_afterAuthentication_overTcp_returnsCorrelatedEntries()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Reports"))));

        const QString alphaPath = QDir(storageRoot).filePath(QStringLiteral("Documents/alpha.txt"));
        QFile alphaFile(alphaPath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("alpha"), qint64{5});
        alphaFile.close();

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(repositoryFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        ServerController controller(repositoryFilePath, storageRoot);

        const LicenseManagerResult initializeResult = controller.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QVERIFY(controller.startListening(QHostAddress::LocalHost, 0));
        QVERIFY(controller.serverPort() != 0);

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, controller.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        const AuthenticateRequestData authenticationData{license.productKey, license.deviceId};
        const AuthenticationEncodeResult authenticationPayload = MiniCloud::Protocol::serializeAuthenticateRequest(authenticationData);
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr RequestId authenticationRequestId = 801;
        constexpr TaskId taskId = 0;
        const FrameEncodeResult encodedAuthentication = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateRequest,
            authenticationRequestId,
            taskId,
            authenticationPayload.payload);

        QCOMPARE(encodedAuthentication.status, FrameEncodeStatus::Success);

        QCOMPARE(
            clientSocket.write(encodedAuthentication.encodedFrame), static_cast<qint64>(encodedAuthentication.encodedFrame.size()));

        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse = authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(authenticationResponse.frame.header.requestId, authenticationRequestId);
        QCOMPARE(authenticationResponse.frame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decodedAuthentication = MiniCloud::Protocol::deserializeAuthenticateResponse(
            authenticationResponse.frame.payload);

        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);

        const MiniCloud::Protocol::FileProtocolEncodeResult browsePayload = MiniCloud::Protocol::serializeBrowseRequest({QStringLiteral("/Documents")});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId browseRequestId = 802;
        const FrameEncodeResult encodedBrowse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseRequest,
            browseRequestId,
            taskId,
            browsePayload.payload);
        QCOMPARE(encodedBrowse.status, FrameEncodeStatus::Success);

        QCOMPARE(clientSocket.write(encodedBrowse.encodedFrame), static_cast<qint64>(encodedBrowse.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser browseResponseParser;
        browseResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult browseResponse = browseResponseParser.tryTakeFrame();
        QCOMPARE(browseResponse.status, FrameParser::FrameParseStatus::FrameReady);

        const MiniCloud::Protocol::ProtocolFrame &responseFrame = browseResponse.frame;
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

        controller.stop();
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);
    }

    void uploadSequentialChunks_afterAuthentication_overTcp_commitsAndReturnsCorrelatedCompletion()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(repositoryFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        ServerController controller(repositoryFilePath, storageRoot);
        QCOMPARE(controller.initialize().status, LicenseManagerOperationStatus::Success);
        QVERIFY(controller.startListening(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, controller.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        constexpr RequestId authenticationRequestId = 901;
        const FrameEncodeResult encodedAuthentication = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateRequest,
            authenticationRequestId,
            taskId,
            authenticationPayload.payload);
        QCOMPARE(encodedAuthentication.status, FrameEncodeStatus::Success);
        QCOMPARE(
            clientSocket.write(encodedAuthentication.encodedFrame),
            static_cast<qint64>(encodedAuthentication.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse =
            authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);
        QCOMPARE(authenticationResponse.frame.header.requestId, authenticationRequestId);

        const AuthenticateResponseDecodeResult decodedAuthentication =
            MiniCloud::Protocol::deserializeAuthenticateResponse(authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);

        const MiniCloud::Protocol::FileProtocolEncodeResult uploadStartPayload =
            MiniCloud::Protocol::serializeUploadStartRequest(
                {QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5});
        QCOMPARE(uploadStartPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId uploadRequestId = 902;
        const FrameEncodeResult encodedUploadStart = MiniCloud::Protocol::serializeFrame(
            MessageType::UploadStartRequest,
            uploadRequestId,
            taskId,
            uploadStartPayload.payload);
        QCOMPARE(encodedUploadStart.status, FrameEncodeStatus::Success);
        QCOMPARE(
            clientSocket.write(encodedUploadStart.encodedFrame),
            static_cast<qint64>(encodedUploadStart.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser uploadReadyParser;
        uploadReadyParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult uploadReadyResponse = uploadReadyParser.tryTakeFrame();
        QCOMPARE(uploadReadyResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(uploadReadyResponse.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(uploadReadyResponse.frame.header.requestId, uploadRequestId);
        QCOMPARE(uploadReadyResponse.frame.header.taskId, taskId);

        const MiniCloud::Protocol::UploadReadyResponseDecodeResult decodedReady =
            MiniCloud::Protocol::deserializeUploadReadyResponse(uploadReadyResponse.frame.payload);
        QCOMPARE(decodedReady.status, MiniCloud::Protocol::UploadReadyResponseDecodeResult::Status::Success);
        QCOMPARE(decodedReady.data.path, QStringLiteral("/Documents/report.bin"));

        const MiniCloud::Protocol::FileProtocolEncodeResult firstChunkPayload =
            MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("abc")});
        QCOMPARE(firstChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult encodedFirstChunk = MiniCloud::Protocol::serializeFrame(
            MessageType::FileChunk,
            uploadRequestId,
            taskId,
            firstChunkPayload.payload);
        QCOMPARE(encodedFirstChunk.status, FrameEncodeStatus::Success);
        QCOMPARE(
            clientSocket.write(encodedFirstChunk.encodedFrame),
            static_cast<qint64>(encodedFirstChunk.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        const MiniCloud::Protocol::FileProtocolEncodeResult finalChunkPayload =
            MiniCloud::Protocol::serializeFileChunk({3, QByteArrayLiteral("de")});
        QCOMPARE(finalChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult encodedFinalChunk = MiniCloud::Protocol::serializeFrame(
            MessageType::FileChunk,
            uploadRequestId,
            taskId,
            finalChunkPayload.payload);
        QCOMPARE(encodedFinalChunk.status, FrameEncodeStatus::Success);
        QCOMPARE(
            clientSocket.write(encodedFinalChunk.encodedFrame),
            static_cast<qint64>(encodedFinalChunk.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser completionResponseParser;
        completionResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult completionResponse =
            completionResponseParser.tryTakeFrame();
        QCOMPARE(completionResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(completionResponse.frame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(completionResponse.frame.header.requestId, uploadRequestId);
        QCOMPARE(completionResponse.frame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult decodedCompletion =
            MiniCloud::Protocol::deserializeFileOperationResponse(completionResponse.frame.payload);
        QCOMPARE(decodedCompletion.status, MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decodedCompletion.data.path, QStringLiteral("/Documents/report.bin"));

        const QString reportNativePath =
            QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        QFile reportFile(reportNativePath);
        QVERIFY(reportFile.open(QIODevice::ReadOnly));
        QCOMPARE(reportFile.readAll(), QByteArrayLiteral("abcde"));

        controller.stop();
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);
    }

    void downloadRequest_afterAuthentication_overTcp_returnsStartChunksAndCompletion()
    {
        constexpr qsizetype maximumChunkSize = 64 * 1024;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QByteArray expectedContent =
            QByteArray(maximumChunkSize, 'a') + QByteArrayLiteral("z");
        const QString sourceFilePath =
            QDir(storageRoot).filePath(QStringLiteral("Documents/large.bin"));
        QFile sourceFile(sourceFilePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write(expectedContent),
                 static_cast<qint64>(expectedContent.size()));
        sourceFile.close();

        const LicenseRecord license{
            QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(repositoryFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        ServerController controller(repositoryFilePath, storageRoot);
        QCOMPARE(controller.initialize().status, LicenseManagerOperationStatus::Success);
        QVERIFY(controller.startListening(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, controller.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest(
                {license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        constexpr TaskId taskId = 0;
        constexpr RequestId authenticationRequestId = 1101;
        const FrameEncodeResult encodedAuthentication =
            MiniCloud::Protocol::serializeFrame(
                MessageType::AuthenticateRequest,
                authenticationRequestId,
                taskId,
                authenticationPayload.payload);
        QCOMPARE(encodedAuthentication.status, FrameEncodeStatus::Success);
        QCOMPARE(
            clientSocket.write(encodedAuthentication.encodedFrame),
            static_cast<qint64>(encodedAuthentication.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        QTRY_VERIFY(clientSocket.bytesAvailable() > 0);
        FrameParser authenticationResponseParser;
        authenticationResponseParser.appendData(clientSocket.readAll());
        const FrameParser::FrameParseResult authenticationResponse =
            authenticationResponseParser.tryTakeFrame();
        QCOMPARE(authenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(authenticationResponse.frame.header.messageType,
                 MessageType::AuthenticateResponse);
        QCOMPARE(authenticationResponse.frame.header.requestId,
                 authenticationRequestId);
        QCOMPARE(authenticationResponse.frame.header.taskId, taskId);

        const AuthenticateResponseDecodeResult decodedAuthentication =
            MiniCloud::Protocol::deserializeAuthenticateResponse(
                authenticationResponse.frame.payload);
        QCOMPARE(decodedAuthentication.status,
                 AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedAuthentication.data.status, AuthenticationStatus::Valid);

        const MiniCloud::Protocol::FileProtocolEncodeResult downloadPayload =
            MiniCloud::Protocol::serializeDownloadRequest(
                {QStringLiteral("/Documents/large.bin")});
        QCOMPARE(downloadPayload.status,
                 MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId downloadRequestId = 1102;
        const FrameEncodeResult encodedDownload =
            MiniCloud::Protocol::serializeFrame(
                MessageType::DownloadRequest,
                downloadRequestId,
                taskId,
                downloadPayload.payload);
        QCOMPARE(encodedDownload.status, FrameEncodeStatus::Success);
        QCOMPARE(
            clientSocket.write(encodedDownload.encodedFrame),
            static_cast<qint64>(encodedDownload.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());

        FrameParser responseParser;
        QList<MiniCloud::Protocol::ProtocolFrame> responseFrames;
        const auto collectResponseFrames = [&responseParser, &responseFrames, &clientSocket]()
        {
            responseParser.appendData(clientSocket.readAll());

            while (true)
            {
                const FrameParser::FrameParseResult frameResult =
                    responseParser.tryTakeFrame();

                if (frameResult.status != FrameParser::FrameParseStatus::FrameReady)
                {
                    return;
                }

                responseFrames.append(frameResult.frame);
            }
        };

        QTRY_VERIFY_WITH_TIMEOUT(
            (collectResponseFrames(), responseFrames.size() == 4), 1000);

        const MiniCloud::Protocol::ProtocolFrame &startFrame = responseFrames.at(0);
        QCOMPARE(startFrame.header.messageType, MessageType::DownloadStartResponse);
        QCOMPARE(startFrame.header.requestId, downloadRequestId);
        QCOMPARE(startFrame.header.taskId, taskId);

        const MiniCloud::Protocol::DownloadStartResponseDecodeResult startDecoded =
            MiniCloud::Protocol::deserializeDownloadStartResponse(startFrame.payload);
        QCOMPARE(startDecoded.status,
                 MiniCloud::Protocol::DownloadStartResponseDecodeResult::Status::Success);
        QCOMPARE(startDecoded.data.path, QStringLiteral("/Documents/large.bin"));
        QCOMPARE(startDecoded.data.totalSizeBytes, quint64{65537});

        const MiniCloud::Protocol::ProtocolFrame &firstChunkFrame = responseFrames.at(1);
        QCOMPARE(firstChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(firstChunkFrame.header.requestId, downloadRequestId);
        QCOMPARE(firstChunkFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileChunkDecodeResult firstChunkDecoded =
            MiniCloud::Protocol::deserializeFileChunk(firstChunkFrame.payload);
        QCOMPARE(firstChunkDecoded.status,
                 MiniCloud::Protocol::FileChunkDecodeResult::Status::Success);
        QCOMPARE(firstChunkDecoded.data.offset, quint64{0});
        QCOMPARE(firstChunkDecoded.data.bytes.size(), maximumChunkSize);
        QCOMPARE(firstChunkDecoded.data.bytes, expectedContent.left(maximumChunkSize));

        const MiniCloud::Protocol::ProtocolFrame &finalChunkFrame = responseFrames.at(2);
        QCOMPARE(finalChunkFrame.header.messageType, MessageType::FileChunk);
        QCOMPARE(finalChunkFrame.header.requestId, downloadRequestId);
        QCOMPARE(finalChunkFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileChunkDecodeResult finalChunkDecoded =
            MiniCloud::Protocol::deserializeFileChunk(finalChunkFrame.payload);
        QCOMPARE(finalChunkDecoded.status,
                 MiniCloud::Protocol::FileChunkDecodeResult::Status::Success);
        QCOMPARE(finalChunkDecoded.data.offset, quint64{65536});
        QCOMPARE(finalChunkDecoded.data.bytes, QByteArrayLiteral("z"));
        QCOMPARE(firstChunkDecoded.data.bytes + finalChunkDecoded.data.bytes,
                 expectedContent);

        const MiniCloud::Protocol::ProtocolFrame &completionFrame = responseFrames.at(3);
        QCOMPARE(completionFrame.header.messageType,
                 MessageType::FileOperationResponse);
        QCOMPARE(completionFrame.header.requestId, downloadRequestId);
        QCOMPARE(completionFrame.header.taskId, taskId);

        const MiniCloud::Protocol::FileOperationResponseDecodeResult completionDecoded =
            MiniCloud::Protocol::deserializeFileOperationResponse(completionFrame.payload);
        QCOMPARE(completionDecoded.status,
                 MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(completionDecoded.data.path, QStringLiteral("/Documents/large.bin"));

        QFile unchangedSourceFile(sourceFilePath);
        QVERIFY(unchangedSourceFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedSourceFile.readAll(), expectedContent);

        controller.stop();
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);
    }

    void clientDisconnect_duringUpload_discardsTemporaryFileAndAllowsCleanRestart()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString repositoryFilePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(repositoryFilePath);
        QCOMPARE(repository.insert(license).status, LicenseRepositoryStatus::Success);

        ServerController controller(repositoryFilePath, storageRoot);
        QCOMPARE(controller.initialize().status, LicenseManagerOperationStatus::Success);
        QVERIFY(controller.startListening(QHostAddress::LocalHost, 0));

        constexpr TaskId taskId = 0;
        const AuthenticationEncodeResult authenticationPayload =
            MiniCloud::Protocol::serializeAuthenticateRequest({license.productKey, license.deviceId});
        QCOMPARE(authenticationPayload.status, AuthenticationEncodeResult::Status::Success);

        QTcpSocket firstClientSocket;
        firstClientSocket.connectToHost(QHostAddress::LocalHost, controller.serverPort());
        QTRY_COMPARE(firstClientSocket.state(), QAbstractSocket::ConnectedState);

        constexpr RequestId firstAuthenticationRequestId = 1001;
        const FrameEncodeResult encodedFirstAuthentication = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateRequest,
            firstAuthenticationRequestId,
            taskId,
            authenticationPayload.payload);
        QCOMPARE(encodedFirstAuthentication.status, FrameEncodeStatus::Success);
        QCOMPARE(
            firstClientSocket.write(encodedFirstAuthentication.encodedFrame),
            static_cast<qint64>(encodedFirstAuthentication.encodedFrame.size()));
        QVERIFY(firstClientSocket.waitForBytesWritten());

        QTRY_VERIFY(firstClientSocket.bytesAvailable() > 0);
        FrameParser firstAuthenticationResponseParser;
        firstAuthenticationResponseParser.appendData(firstClientSocket.readAll());
        const FrameParser::FrameParseResult firstAuthenticationResponse =
            firstAuthenticationResponseParser.tryTakeFrame();
        QCOMPARE(firstAuthenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstAuthenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedFirstAuthentication =
            MiniCloud::Protocol::deserializeAuthenticateResponse(firstAuthenticationResponse.frame.payload);
        QCOMPARE(decodedFirstAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedFirstAuthentication.data.status, AuthenticationStatus::Valid);

        const MiniCloud::Protocol::FileProtocolEncodeResult uploadStartPayload =
            MiniCloud::Protocol::serializeUploadStartRequest(
                {QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5});
        QCOMPARE(uploadStartPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId firstUploadRequestId = 1002;
        const FrameEncodeResult encodedFirstUploadStart = MiniCloud::Protocol::serializeFrame(
            MessageType::UploadStartRequest,
            firstUploadRequestId,
            taskId,
            uploadStartPayload.payload);
        QCOMPARE(encodedFirstUploadStart.status, FrameEncodeStatus::Success);
        QCOMPARE(
            firstClientSocket.write(encodedFirstUploadStart.encodedFrame),
            static_cast<qint64>(encodedFirstUploadStart.encodedFrame.size()));
        QVERIFY(firstClientSocket.waitForBytesWritten());

        QTRY_VERIFY(firstClientSocket.bytesAvailable() > 0);
        FrameParser firstUploadReadyParser;
        firstUploadReadyParser.appendData(firstClientSocket.readAll());
        const FrameParser::FrameParseResult firstUploadReadyResponse =
            firstUploadReadyParser.tryTakeFrame();
        QCOMPARE(firstUploadReadyResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstUploadReadyResponse.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(firstUploadReadyResponse.frame.header.requestId, firstUploadRequestId);

        const MiniCloud::Protocol::FileProtocolEncodeResult firstChunkPayload =
            MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("abc")});
        QCOMPARE(firstChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult encodedFirstChunk = MiniCloud::Protocol::serializeFrame(
            MessageType::FileChunk,
            firstUploadRequestId,
            taskId,
            firstChunkPayload.payload);
        QCOMPARE(encodedFirstChunk.status, FrameEncodeStatus::Success);
        QCOMPARE(
            firstClientSocket.write(encodedFirstChunk.encodedFrame),
            static_cast<qint64>(encodedFirstChunk.encodedFrame.size()));
        QVERIFY(firstClientSocket.waitForBytesWritten());

        const MiniCloud::Protocol::FileProtocolEncodeResult browsePayload =
            MiniCloud::Protocol::serializeBrowseRequest({QStringLiteral("/Documents")});
        QCOMPARE(browsePayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        constexpr RequestId browseRequestId = 1003;
        const FrameEncodeResult encodedBrowse = MiniCloud::Protocol::serializeFrame(
            MessageType::BrowseRequest,
            browseRequestId,
            taskId,
            browsePayload.payload);
        QCOMPARE(encodedBrowse.status, FrameEncodeStatus::Success);
        QCOMPARE(
            firstClientSocket.write(encodedBrowse.encodedFrame),
            static_cast<qint64>(encodedBrowse.encodedFrame.size()));
        QVERIFY(firstClientSocket.waitForBytesWritten());

        QTRY_VERIFY(firstClientSocket.bytesAvailable() > 0);
        FrameParser browseResponseParser;
        browseResponseParser.appendData(firstClientSocket.readAll());
        const FrameParser::FrameParseResult browseResponse = browseResponseParser.tryTakeFrame();
        QCOMPARE(browseResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(browseResponse.frame.header.messageType, MessageType::BrowseResponse);
        QCOMPARE(browseResponse.frame.header.requestId, browseRequestId);

        const BrowseResponseDecodeResult decodedBrowse =
            MiniCloud::Protocol::deserializeBrowseResponse(browseResponse.frame.payload);
        QCOMPARE(decodedBrowse.status, BrowseResponseDecodeResult::Status::Success);
        QVERIFY(decodedBrowse.data.entries.isEmpty());

        firstClientSocket.disconnectFromHost();
        QTRY_COMPARE(firstClientSocket.state(), QAbstractSocket::UnconnectedState);

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        const QString reportNativePath = documentsDirectory.filePath(QStringLiteral("report.bin"));
        QTRY_VERIFY(!QFileInfo::exists(reportNativePath));
        QTRY_VERIFY(documentsDirectory.entryList(
                        QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden)
                        .isEmpty());

        QTcpSocket secondClientSocket;
        secondClientSocket.connectToHost(QHostAddress::LocalHost, controller.serverPort());
        QTRY_COMPARE(secondClientSocket.state(), QAbstractSocket::ConnectedState);

        constexpr RequestId secondAuthenticationRequestId = 1004;
        const FrameEncodeResult encodedSecondAuthentication = MiniCloud::Protocol::serializeFrame(
            MessageType::AuthenticateRequest,
            secondAuthenticationRequestId,
            taskId,
            authenticationPayload.payload);
        QCOMPARE(encodedSecondAuthentication.status, FrameEncodeStatus::Success);
        QCOMPARE(
            secondClientSocket.write(encodedSecondAuthentication.encodedFrame),
            static_cast<qint64>(encodedSecondAuthentication.encodedFrame.size()));
        QVERIFY(secondClientSocket.waitForBytesWritten());

        QTRY_VERIFY(secondClientSocket.bytesAvailable() > 0);
        FrameParser secondAuthenticationResponseParser;
        secondAuthenticationResponseParser.appendData(secondClientSocket.readAll());
        const FrameParser::FrameParseResult secondAuthenticationResponse =
            secondAuthenticationResponseParser.tryTakeFrame();
        QCOMPARE(secondAuthenticationResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondAuthenticationResponse.frame.header.messageType, MessageType::AuthenticateResponse);

        const AuthenticateResponseDecodeResult decodedSecondAuthentication =
            MiniCloud::Protocol::deserializeAuthenticateResponse(secondAuthenticationResponse.frame.payload);
        QCOMPARE(decodedSecondAuthentication.status, AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodedSecondAuthentication.data.status, AuthenticationStatus::Valid);

        constexpr RequestId restartedUploadRequestId = 1005;
        const FrameEncodeResult encodedRestartedUploadStart = MiniCloud::Protocol::serializeFrame(
            MessageType::UploadStartRequest,
            restartedUploadRequestId,
            taskId,
            uploadStartPayload.payload);
        QCOMPARE(encodedRestartedUploadStart.status, FrameEncodeStatus::Success);
        QCOMPARE(
            secondClientSocket.write(encodedRestartedUploadStart.encodedFrame),
            static_cast<qint64>(encodedRestartedUploadStart.encodedFrame.size()));
        QVERIFY(secondClientSocket.waitForBytesWritten());

        QTRY_VERIFY(secondClientSocket.bytesAvailable() > 0);
        FrameParser restartedUploadReadyParser;
        restartedUploadReadyParser.appendData(secondClientSocket.readAll());
        const FrameParser::FrameParseResult restartedUploadReadyResponse =
            restartedUploadReadyParser.tryTakeFrame();
        QCOMPARE(restartedUploadReadyResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(restartedUploadReadyResponse.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(restartedUploadReadyResponse.frame.header.requestId, restartedUploadRequestId);

        const MiniCloud::Protocol::FileProtocolEncodeResult completedChunkPayload =
            MiniCloud::Protocol::serializeFileChunk({0, QByteArrayLiteral("abcde")});
        QCOMPARE(completedChunkPayload.status, MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult encodedCompletedChunk = MiniCloud::Protocol::serializeFrame(
            MessageType::FileChunk,
            restartedUploadRequestId,
            taskId,
            completedChunkPayload.payload);
        QCOMPARE(encodedCompletedChunk.status, FrameEncodeStatus::Success);
        QCOMPARE(
            secondClientSocket.write(encodedCompletedChunk.encodedFrame),
            static_cast<qint64>(encodedCompletedChunk.encodedFrame.size()));
        QVERIFY(secondClientSocket.waitForBytesWritten());

        QTRY_VERIFY(secondClientSocket.bytesAvailable() > 0);
        FrameParser completionResponseParser;
        completionResponseParser.appendData(secondClientSocket.readAll());
        const FrameParser::FrameParseResult completionResponse =
            completionResponseParser.tryTakeFrame();
        QCOMPARE(completionResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(completionResponse.frame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(completionResponse.frame.header.requestId, restartedUploadRequestId);

        QFile reportFile(reportNativePath);
        QVERIFY(reportFile.open(QIODevice::ReadOnly));
        QCOMPARE(reportFile.readAll(), QByteArrayLiteral("abcde"));

        controller.stop();
        QTRY_COMPARE(secondClientSocket.state(), QAbstractSocket::UnconnectedState);
    }
};

QTEST_GUILESS_MAIN(ServerControllerTest)
#include "servercontrollertest.moc"
