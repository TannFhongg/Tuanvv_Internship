#include <QtTest/QTest>
#include <QTcpServer>
#include "NetworkClient.h"
#include <QSignalSpy>
#include <QTcpSocket>
#include "protocolconstants.h"
#include "protocolcodec.h"
#include "frameparser.h"
class NetworkClientTest : public QObject
{
    Q_OBJECT

private slots:
    void initialState_isUnconnected()
    {
        NetworkClient networkClient;

        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spyConnectionError.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);

        networkClient.disconnectFromServer();
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spyConnectionError.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);
    }

    void connectToServer_invalidArguments_returnsFalse()
    {
        NetworkClient networkClient;

        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        quint16 port = 12345;
        networkClient.connectToServer("", port);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);

        QString hostname = "127.0.0.1";
        networkClient.connectToServer(hostname, 0);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spystateChanged.count(), 0);
    }

    void connectToServer_listeningServer_emitsConnected()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);

        NetworkClient networkClient;
        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        QSignalSpy spynewConnection(&server, &QTcpServer::newConnection);

        quint16 port = server.serverPort();
        QString hostname = "127.0.0.1";

        QVERIFY(networkClient.connectToServer(hostname, port) == true);

        QTRY_COMPARE(spyConnected.count(), 1);
        QTRY_COMPARE(spynewConnection.count(), 1);

        QTRY_COMPARE(networkClient.isConnected(), true);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnectionError.count(), 0);

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        networkClient.disconnectFromServer();
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);

        serverSocket->close();
    }

    void disconnectFromServer_connectedClient_emitsDisconnected()
    {

        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);

        NetworkClient networkClient;
        quint16 port = server.serverPort();
        QString hostname = "127.0.0.1";
        QVERIFY(networkClient.connectToServer(hostname, port) == true);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        QSignalSpy spyClientDisconnected(serverSocket, &QTcpSocket::disconnected);

        networkClient.disconnectFromServer();

        QTRY_COMPARE(spyDisconnected.count(), 1);
        QTRY_COMPARE(spyClientDisconnected.count(), 1);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(networkClient.isConnected() == false);
        QTRY_COMPARE(spyConnectionError.count(), 0);
    }

    void connectToServer_whenAlreadyConnected_returnsFalse()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        NetworkClient networkClient;
        quint16 port = server.serverPort();
        QString hostname = "127.0.0.1";
        QVERIFY(networkClient.connectToServer(hostname, port) == true);

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(networkClient.isConnected(), true);

        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);

        QSignalSpy spynewConnection(&server, &QTcpServer::newConnection);

        QVERIFY(networkClient.connectToServer(hostname, port) == false);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);
        QCOMPARE(spyConnected.count(), 0);
        QCOMPARE(spyDisconnected.count(), 0);
        QCOMPARE(spyConnectionError.count(), 0);

        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        networkClient.disconnectFromServer();
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
    }

    void connectToServer_closedPort_emitsConnectionError()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        quint16 closedPort = server.serverPort();
        server.close();

        NetworkClient networkClient;
        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        QVERIFY(networkClient.connectToServer("127.0.0.1", closedPort));

        QTRY_VERIFY(spyConnectionError.count() == 1);

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);

        QTRY_COMPARE(networkClient.errorString().isEmpty(), false);

        QTRY_VERIFY(spystateChanged.count() > 0);
    }

    void connectToServer_afterFailure_canConnectSuccessfully()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        quint16 port = server.serverPort();
        server.close();

        NetworkClient networkClient;
        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        networkClient.connectToServer("127.0.0.1", port);
        QTRY_VERIFY(spyConnectionError.count());
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);

        QTcpServer newServer;
        newServer.listen(QHostAddress::LocalHost, 0);

        spyConnected.clear();
        spyConnectionError.clear();
        spystateChanged.clear();

        QSignalSpy spynewConnection(&newServer, &QTcpServer::newConnection);

        networkClient.connectToServer("127.0.0.1", newServer.serverPort());

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);

        QTRY_COMPARE(spyConnected.count(), 1);
        QTRY_COMPARE(spynewConnection.count(), 1);

        networkClient.disconnectFromServer();
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
    }

    void serverDisconnect_connectedClient_emitsDisconnected()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        NetworkClient networkClient;
        quint16 port = server.serverPort();
        QVERIFY(networkClient.connectToServer("127.0.0.1", port));
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);

        serverSocket->disconnectFromHost();

        QTRY_COMPARE(spyDisconnected.count(), 1);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(networkClient.isConnected(), false);
        QTRY_COMPARE(networkClient.errorString().isEmpty(), false);
        QTRY_COMPARE(spyConnectionError.count(), 1);
    }

    void sendFrame_whenDisconnected_returnsFalse()
    {
        NetworkClient networkClient;
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(networkClient.isConnected() == false);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray::fromHex("DE 00 AD FF 01");

        QSignalSpy spyConnected(&networkClient, &NetworkClient::connected);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spystateChanged(&networkClient, &NetworkClient::stateChanged);
        QSignalSpy spyprotocolError(&networkClient, &NetworkClient::protocolError);

        bool result = networkClient.sendFrame(messageType, requestId, taskId, payload);

        QVERIFY(result == false);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(networkClient.isConnected() == false);
        QTRY_COMPARE(spyConnected.count(), 0);
        QTRY_COMPARE(spyDisconnected.count(), 0);
        QTRY_COMPARE(spyConnectionError.count(), 0);
        QTRY_COMPARE(spystateChanged.count(), 0);
        QTRY_COMPARE(spyprotocolError.count(), 0);
    }

    void sendFrame_whenConnected_serverReceivesCompleteFrame()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        NetworkClient networkClient;
        networkClient.connectToServer("127.0.0.1", server.serverPort());
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray::fromHex("DE 00 AD FF 01");

        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spyprotocolError(&networkClient, &NetworkClient::protocolError);

        bool result = networkClient.sendFrame(messageType, requestId, taskId, payload);
        QVERIFY(result == true);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);

        QTRY_VERIFY(serverSocket->bytesAvailable() == MiniCloud::Protocol::protocolWireHeaderSize + payload.size());

        QByteArray receivedData = serverSocket->readAll();
        MiniCloud::Protocol::HeaderDecodeResult headerResult = MiniCloud::Protocol::deserializeHeader(receivedData.left(MiniCloud::Protocol::protocolWireHeaderSize));

        QCOMPARE(headerResult.status, MiniCloud::Protocol::HeaderDecodeStatus::Success);
        QCOMPARE(headerResult.header.messageType, messageType);
        QCOMPARE(headerResult.header.requestId, requestId);
        QCOMPARE(headerResult.header.taskId, taskId);
        QCOMPARE(headerResult.header.payloadLength, static_cast<quint32>(payload.size()));

        QByteArray receivedPayload = receivedData.mid(MiniCloud::Protocol::protocolWireHeaderSize);
        QCOMPARE(receivedPayload, payload);

        QTRY_COMPARE(spyDisconnected.count(), 0);
        QTRY_COMPARE(spyConnectionError.count(), 0);
        QTRY_COMPARE(spyprotocolError.count(), 0);

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);
    }

    void sendFrame_invalidMessageType_returnsFalseAndSendsNoBytes()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        NetworkClient networkClient;

        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::Invalid;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray::fromHex("DE 00 AD FF 01");

        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spyprotocolError(&networkClient, &NetworkClient::protocolError);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyReadyRead(serverSocket, &QTcpSocket::readyRead);

        bool result = networkClient.sendFrame(messageType, requestId, taskId, payload);
        QVERIFY(result == false);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);
        QTest::qWait(100);
        QTRY_COMPARE(serverSocket->bytesAvailable(), 0);

        QTRY_COMPARE(spyConnectionError.count(), 0);
        QTRY_COMPARE(spyprotocolError.count(), 0);
        QTRY_COMPARE(spyDisconnected.count(), 0);
    }

    void sendFrame_payloadTooLarge_returnsFalseAndSendsNoBytes()
    {

        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray(MiniCloud::Protocol::protocolMaxControlPayloadSize + 1, 'A');
        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spyprotocolError(&networkClient, &NetworkClient::protocolError);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyReadyRead(serverSocket, &QTcpSocket::readyRead);

        bool result = networkClient.sendFrame(messageType, requestId, taskId, payload);
        QVERIFY(result == false);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);

        QTRY_COMPARE(spyReadyRead.count(), 0);
        QTRY_COMPARE(spyConnectionError.count(), 0);
        QTRY_COMPARE(spyprotocolError.count(), 0);
        QTRY_COMPARE(spyDisconnected.count(), 0);

        QTRY_COMPARE(serverSocket->bytesAvailable(), 0);
    }

    void sendFrame_payloadAtMaximumSize_serverReceivesCompleteFrame()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray(MiniCloud::Protocol::protocolMaxControlPayloadSize, 'A');

        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spyprotocolError(&networkClient, &NetworkClient::protocolError);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spyReadyRead(serverSocket, &QTcpSocket::readyRead);

        QVERIFY(networkClient.sendFrame(messageType, requestId, taskId, payload));
        QTRY_VERIFY(spyReadyRead.count() > 0);

        QTRY_COMPARE(serverSocket->bytesAvailable(), MiniCloud::Protocol::protocolWireHeaderSize + payload.size());

        QByteArray receivedData = serverSocket->readAll();

        MiniCloud::Protocol::FrameParser parser;
        parser.appendData(receivedData);

        MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();

        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result.frame.header.messageType, messageType);
        QCOMPARE(result.frame.header.requestId, requestId);
        QCOMPARE(result.frame.header.taskId, taskId);
        QCOMPARE(result.frame.header.payloadLength, MiniCloud::Protocol::protocolMaxControlPayloadSize);
        QCOMPARE(result.frame.payload, payload);

        QTRY_COMPARE(spyConnectionError.count(), 0);
        QTRY_COMPARE(spyprotocolError.count(), 0);
        QTRY_COMPARE(spyDisconnected.count(), 0);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(parser.bufferedSize(), 0);
    }

    void sendFrame_twoConsecutiveFrames_serverReceivesBothInOrder()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);

        MiniCloud::Protocol::MessageType messageType_A = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId_A = 101;
        MiniCloud::Protocol::TaskId taskId_A = 900;
        QByteArray payload_A = QByteArray::fromHex("DE 00 AD FF 01");

        MiniCloud::Protocol::MessageType messageType_B = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId_B = 102;
        MiniCloud::Protocol::TaskId taskId_B = 901;
        QByteArray payload_B = QByteArray::fromHex("42 42 42 42 42");

        QSignalSpy spyConnectionError(&networkClient, &NetworkClient::connectionError);
        QSignalSpy spyprotocolError(&networkClient, &NetworkClient::protocolError);
        QSignalSpy spyDisconnected(&networkClient, &NetworkClient::disconnected);
        QSignalSpy spySocketError(serverSocket, &QTcpSocket::errorOccurred);

        bool resultFrame_A = networkClient.sendFrame(messageType_A, requestId_A, taskId_A, payload_A);
        QVERIFY(resultFrame_A == true);

        bool resultFrame_B = networkClient.sendFrame(messageType_B, requestId_B, taskId_B, payload_B);
        QVERIFY(resultFrame_B == true);

        QTRY_COMPARE(serverSocket->bytesAvailable(), MiniCloud::Protocol::protocolWireHeaderSize + payload_A.size() + MiniCloud::Protocol::protocolWireHeaderSize + payload_B.size());
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);

        QTRY_COMPARE(spyConnectionError.count(), 0);
        QTRY_COMPARE(spyprotocolError.count(), 0);
        QTRY_COMPARE(spyDisconnected.count(), 0);
        QTRY_COMPARE(spySocketError.count(), 0);

        QByteArray receivedData = serverSocket->readAll();

        MiniCloud::Protocol::FrameParser parser;
        parser.appendData(receivedData);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_A = parser.tryTakeFrame();
        QCOMPARE(result_A.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_A.frame.header.messageType, messageType_A);
        QCOMPARE(result_A.frame.header.requestId, requestId_A);
        QCOMPARE(result_A.frame.header.taskId, taskId_A);
        QCOMPARE(result_A.frame.header.payloadLength, payload_A.size());
        QCOMPARE(result_A.frame.payload, payload_A);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_B = parser.tryTakeFrame();
        QCOMPARE(result_B.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_B.frame.header.messageType, messageType_B);
        QCOMPARE(result_B.frame.header.requestId, requestId_B);
        QCOMPARE(result_B.frame.header.taskId, taskId_B);
        QCOMPARE(result_B.frame.header.payloadLength, payload_B.size());
        QCOMPARE(result_B.frame.payload, payload_B);

        QTRY_COMPARE(parser.bufferedSize(), 0);

    }

    
};

QTEST_GUILESS_MAIN(NetworkClientTest)
#include "networkclienttest.moc"