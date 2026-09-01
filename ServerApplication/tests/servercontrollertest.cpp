#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QTcpSocket>

#include "authentication.h"
#include "frameparser.h"
#include "licenserecord.h"
#include "licenserepository.h"
#include "protocolcodec.h"
#include "servercontroller.h"

using MiniCloud::Protocol::AuthenticateRequestData;
using MiniCloud::Protocol::AuthenticateResponseDecodeResult;
using MiniCloud::Protocol::AuthenticationEncodeResult;
using MiniCloud::Protocol::AuthenticationStatus;
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

        const LicenseRecord license{QStringLiteral("MCLD-BOUND-0001"), QStringLiteral("DEVICE-OWNER"), true};
        LicenseRepository repository(repositoryFilePath);
        const LicenseRepositoryResult insertResult = repository.insert(license);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        ServerController controller(repositoryFilePath);

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
};

QTEST_GUILESS_MAIN(ServerControllerTest)
#include "servercontrollertest.moc"
