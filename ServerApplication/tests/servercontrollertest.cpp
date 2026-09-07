#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
};

QTEST_GUILESS_MAIN(ServerControllerTest)
#include "servercontrollertest.moc"
