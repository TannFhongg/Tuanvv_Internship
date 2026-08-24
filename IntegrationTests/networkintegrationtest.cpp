#include <QtTest/QTest>

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

#include "NetworkClient.h"
#include "ClientSession.h"
#include "protocolframe.h"
#include "protocoltypes.h"

class NetworkIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip_requestAndResponse_preservesCorrelationIds();
    void initTestCase();
};

void NetworkIntegrationTest::initTestCase()
{

    qRegisterMetaType<MiniCloud::Protocol::ProtocolFrame>(
        "MiniCloud::Protocol::ProtocolFrame");

    qRegisterMetaType<MiniCloud::Protocol::ErrorCode>(
        "MiniCloud::Protocol::ErrorCode");
}
void NetworkIntegrationTest::roundTrip_requestAndResponse_preservesCorrelationIds()
{
    QTcpServer testServer;
    QVERIFY(testServer.listen(QHostAddress::LocalHost, 0));

    NetworkClient networkClient;
    QVERIFY(networkClient.connectToServer(QHostAddress(QHostAddress::LocalHost).toString(), testServer.serverPort()));
    QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
    QVERIFY(networkClient.isConnected());

    QTRY_VERIFY(testServer.hasPendingConnections());
    QTcpSocket *serverSocket = testServer.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);
    QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

    ClientSession session(serverSocket);

    QSignalSpy sessionFrameReceivedSpy(&session, &ClientSession::frameReceived);
    QSignalSpy sessionProtocolErrorSpy(&session, &ClientSession::protocolError);
    QSignalSpy sessionFinishedSpy(&session, &ClientSession::sessionFinished);

    QSignalSpy clientFrameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
    QSignalSpy clientProtocolErrorSpy(&networkClient, &NetworkClient::protocolError);
    QSignalSpy clientDisconnectedSpy(&networkClient, &NetworkClient::disconnected);
    QSignalSpy clientConnectionErrorSpy(&networkClient, &NetworkClient::connectionError);

    QVERIFY(sessionFrameReceivedSpy.isValid());
    QVERIFY(sessionProtocolErrorSpy.isValid());
    QVERIFY(sessionFinishedSpy.isValid());
    QVERIFY(clientFrameReceivedSpy.isValid());
    QVERIFY(clientProtocolErrorSpy.isValid());
    QVERIFY(clientDisconnectedSpy.isValid());
    QVERIFY(clientConnectionErrorSpy.isValid());

    const MiniCloud::Protocol::MessageType requestMessageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
    const MiniCloud::Protocol::RequestId requestId = 0x12345678;
    const MiniCloud::Protocol::TaskId requestTaskId = 0;
    const QByteArray requestPayload = QByteArrayLiteral(R"({"productKey":"test-product-key","deviceId":"test-device"})");

    QVERIFY(networkClient.sendFrame(requestMessageType, requestId, requestTaskId, requestPayload));
    QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
    QVERIFY(networkClient.isConnected());

    QTRY_COMPARE(sessionFrameReceivedSpy.count(), 1);

    const QList<QVariant> requestArguments = sessionFrameReceivedSpy.takeFirst();
    QCOMPARE(requestArguments.size(), 2);
    QCOMPARE(qvariant_cast<ClientSession *>(requestArguments.at(0)), &session);

    const MiniCloud::Protocol::ProtocolFrame receivedRequest = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(requestArguments.at(1));
    QCOMPARE(receivedRequest.header.messageType, requestMessageType);
    QCOMPARE(receivedRequest.header.requestId, requestId);
    QCOMPARE(receivedRequest.header.taskId, requestTaskId);
    QCOMPARE(receivedRequest.header.payloadLength, static_cast<quint32>(requestPayload.size()));
    QCOMPARE(receivedRequest.payload, requestPayload);

    QCOMPARE(sessionProtocolErrorSpy.count(), 0);
    QCOMPARE(sessionFinishedSpy.count(), 0);
    QCOMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

    const MiniCloud::Protocol::MessageType responseMessageType = MiniCloud::Protocol::MessageType::AuthenticateResponse;
    const MiniCloud::Protocol::RequestId responseRequestId = receivedRequest.header.requestId;
    const MiniCloud::Protocol::TaskId responseTaskId = 0;
    const QByteArray responsePayload = QByteArrayLiteral(R"({"success":true})");

    QVERIFY(session.sendFrame(responseMessageType, responseRequestId, responseTaskId, responsePayload));
    QCOMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

    QTRY_COMPARE(clientFrameReceivedSpy.count(), 1);

    const QList<QVariant> responseArguments = clientFrameReceivedSpy.takeFirst();
    QCOMPARE(responseArguments.size(), 1);

    const MiniCloud::Protocol::ProtocolFrame receivedResponse = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(responseArguments.at(0));
    QCOMPARE(receivedResponse.header.messageType, responseMessageType);
    QCOMPARE(receivedResponse.header.requestId, requestId);
    QCOMPARE(receivedResponse.header.taskId, responseTaskId);
    QCOMPARE(receivedResponse.header.payloadLength, static_cast<quint32>(responsePayload.size()));
    QCOMPARE(receivedResponse.payload, responsePayload);

    QCOMPARE(sessionProtocolErrorSpy.count(), 0);
    QCOMPARE(clientProtocolErrorSpy.count(), 0);
    QCOMPARE(clientConnectionErrorSpy.count(), 0);
    QCOMPARE(clientDisconnectedSpy.count(), 0);
    QCOMPARE(sessionFinishedSpy.count(), 0);
    QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
    QVERIFY(networkClient.isConnected());

    networkClient.disconnectFromServer();
    QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
    QTRY_COMPARE(clientDisconnectedSpy.count(), 1);
    QTRY_COMPARE(sessionFinishedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(NetworkIntegrationTest)
#include "networkintegrationtest.moc"
