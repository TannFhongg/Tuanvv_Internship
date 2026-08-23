#include <QtTest/QTest>
#include <QTcpServer>
#include "NetworkClient.h"
#include <QSignalSpy>
#include <QTcpSocket>
#include <QStringList>
#include "protocolconstants.h"
#include "protocolcodec.h"
#include "frameparser.h"
class NetworkClientTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<MiniCloud::Protocol::ProtocolFrame>(
            "MiniCloud::Protocol::ProtocolFrame");
        qRegisterMetaType<MiniCloud::Protocol::ErrorCode>(
            "MiniCloud::Protocol::ErrorCode");
    }

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

    void readyRead_completeFrame_emitsFrameReceived()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray::fromHex("DE 00 AD FF 01");

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QSignalSpy connectionErrorSpy(&networkClient, &NetworkClient::connectionError);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);

        QVERIFY(frameReceivedSpy.isValid());

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        QCOMPARE(serverSocket->write(encodeResult.encodedFrame), static_cast<qint64>(encodeResult.encodedFrame.size()));

        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        const QList<QVariant> signalArguments = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 1);

        const MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(0));

        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QCOMPARE(connectionErrorSpy.count(), 0);
        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());
    }

    void readyRead_partialHeader_waitsForRemainingBytes()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 101;
        MiniCloud::Protocol::TaskId taskId = 900;
        QByteArray payload = QByteArray::fromHex("DE 00 AD FF 01");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        constexpr qsizetype partialHeaderSize = 10;
        const QByteArray firstPart = encodeResult.encodedFrame.left(partialHeaderSize);
        const QByteArray secondPart = encodeResult.encodedFrame.mid(partialHeaderSize);

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QSignalSpy connectionErrorSpy(&networkClient, &NetworkClient::connectionError);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);

        QVERIFY(frameReceivedSpy.isValid());

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QCOMPARE(serverSocket->write(firstPart), static_cast<qint64>(firstPart.size()));
        QVERIFY(!frameReceivedSpy.wait(100));

        QCOMPARE(frameReceivedSpy.count(), 0);
        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());

        QCOMPARE(serverSocket->write(secondPart), static_cast<qint64>(secondPart.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        const QList<QVariant> signalArguments = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 1);

        const MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(0));

        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(connectionErrorSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());
    }

    void readyRead_partialPayload_waitsForRemainingBytes()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        const MiniCloud::Protocol::RequestId requestId = 102;
        const MiniCloud::Protocol::TaskId taskId = 901;
        const QByteArray payload = QByteArray::fromHex("DE AD BE EF 01 02 03 04");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        const qsizetype splitPosition = static_cast<qsizetype>(MiniCloud::Protocol::protocolWireHeaderSize) + 3;
        QVERIFY(splitPosition < encodeResult.encodedFrame.size());

        const QByteArray firstPart = encodeResult.encodedFrame.left(splitPosition);
        const QByteArray secondPart = encodeResult.encodedFrame.mid(splitPosition);

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);

        QVERIFY(frameReceivedSpy.isValid());

        QCOMPARE(serverSocket->write(firstPart), static_cast<qint64>(firstPart.size()));
        QVERIFY(!frameReceivedSpy.wait(100));

        QCOMPARE(frameReceivedSpy.count(), 0);
        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());

        QCOMPARE(serverSocket->write(secondPart), static_cast<qint64>(secondPart.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        const QList<QVariant> signalArguments = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 1);

        const MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(0));

        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());
    }

    void readyRead_twoCompleteFrames_emitsBothInOrder()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::MessageType messageType_A = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        const MiniCloud::Protocol::RequestId requestId_A = 102;
        const MiniCloud::Protocol::TaskId taskId_A = 901;
        const QByteArray payload_A = QByteArray::fromHex("DE AD BE EF 01 02 03 04");

        const MiniCloud::Protocol::MessageType messageType_B = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        const MiniCloud::Protocol::RequestId requestId_B = 103;
        const MiniCloud::Protocol::TaskId taskId_B = 901;
        const QByteArray payload_B = QByteArray::fromHex("DE AD BE EF 01 02 03 04");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult_A = MiniCloud::Protocol::serializeFrame(messageType_A, requestId_A, taskId_A, payload_A);
        QCOMPARE(encodeResult_A.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        const MiniCloud::Protocol::FrameEncodeResult encodeResult_B = MiniCloud::Protocol::serializeFrame(messageType_B, requestId_B, taskId_B, payload_B);
        QCOMPARE(encodeResult_B.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        QByteArray combinedEncodedFrames = encodeResult_A.encodedFrame + encodeResult_B.encodedFrame;

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);

        QCOMPARE(serverSocket->write(combinedEncodedFrames), static_cast<qint64>(combinedEncodedFrames.size()));

        QTRY_COMPARE(frameReceivedSpy.count(), 2);

        const QList<QVariant> signalArguments_A = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments_A.size(), 1);
        const MiniCloud::Protocol::ProtocolFrame receivedFrame_A = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments_A.at(0));
        QCOMPARE(receivedFrame_A.header.messageType, messageType_A);
        QCOMPARE(receivedFrame_A.header.requestId, requestId_A);
        QCOMPARE(receivedFrame_A.header.taskId, taskId_A);
        QCOMPARE(receivedFrame_A.header.payloadLength, static_cast<quint32>(payload_A.size()));
        QCOMPARE(receivedFrame_A.payload, payload_A);

        const QList<QVariant> signalArguments_B = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments_B.size(), 1);
        const MiniCloud::Protocol::ProtocolFrame receivedFrame_B = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments_B.at(0));
        QCOMPARE(receivedFrame_B.header.messageType, messageType_B);
        QCOMPARE(receivedFrame_B.header.requestId, requestId_B);
        QCOMPARE(receivedFrame_B.header.taskId, taskId_B);
        QCOMPARE(receivedFrame_B.header.payloadLength, static_cast<quint32>(payload_B.size()));
        QCOMPARE(receivedFrame_B.payload, payload_B);

        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());
    }

    void readyRead_completeFrameAndPartialNextFrame_preservesRemainder()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::MessageType messageTypeA = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        const MiniCloud::Protocol::RequestId requestIdA = 201;
        const MiniCloud::Protocol::TaskId taskIdA = 1001;
        const QByteArray payloadA = QByteArray::fromHex("01 02 03 04 05");

        const MiniCloud::Protocol::MessageType messageTypeB = MiniCloud::Protocol::MessageType::FileChunk;
        const MiniCloud::Protocol::RequestId requestIdB = 202;
        const MiniCloud::Protocol::TaskId taskIdB = 1002;
        const QByteArray payloadB = QByteArray::fromHex("DE AD BE EF 10 20 30 40");

        const MiniCloud::Protocol::FrameEncodeResult encodeResultA = MiniCloud::Protocol::serializeFrame(messageTypeA, requestIdA, taskIdA, payloadA);
        const MiniCloud::Protocol::FrameEncodeResult encodeResultB = MiniCloud::Protocol::serializeFrame(messageTypeB, requestIdB, taskIdB, payloadB);
        QCOMPARE(encodeResultA.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        QCOMPARE(encodeResultB.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        constexpr qsizetype prefixSize = 10;
        const QByteArray prefixB = encodeResultB.encodedFrame.left(prefixSize);
        const QByteArray suffixB = encodeResultB.encodedFrame.mid(prefixSize);
        const QByteArray firstWrite = encodeResultA.encodedFrame + prefixB;

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);

        QVERIFY(frameReceivedSpy.isValid());

        QCOMPARE(serverSocket->write(firstWrite), static_cast<qint64>(firstWrite.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        const QList<QVariant> argumentsA = frameReceivedSpy.takeFirst();
        QCOMPARE(argumentsA.size(), 1);

        const MiniCloud::Protocol::ProtocolFrame receivedFrameA = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(argumentsA.at(0));

        QCOMPARE(receivedFrameA.header.messageType, messageTypeA);
        QCOMPARE(receivedFrameA.header.requestId, requestIdA);
        QCOMPARE(receivedFrameA.header.taskId, taskIdA);
        QCOMPARE(receivedFrameA.header.payloadLength,
                 static_cast<quint32>(payloadA.size()));
        QCOMPARE(receivedFrameA.payload, payloadA);

        QVERIFY(!frameReceivedSpy.wait(100));
        QCOMPARE(frameReceivedSpy.count(), 0);
        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());

        QCOMPARE(serverSocket->write(suffixB), static_cast<qint64>(suffixB.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        const QList<QVariant> argumentsB = frameReceivedSpy.takeFirst();
        QCOMPARE(argumentsB.size(), 1);

        const MiniCloud::Protocol::ProtocolFrame receivedFrameB = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(argumentsB.at(0));

        QCOMPARE(receivedFrameB.header.messageType, messageTypeB);
        QCOMPARE(receivedFrameB.header.requestId, requestIdB);
        QCOMPARE(receivedFrameB.header.taskId, taskIdB);
        QCOMPARE(receivedFrameB.header.payloadLength, static_cast<quint32>(payloadB.size()));
        QCOMPARE(receivedFrameB.payload, payloadB);

        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());
    }

    void readyRead_invalidMagic_emitsProtocolErrorAndDisconnects()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            301,
            2001,
            QByteArray::fromHex("DE AD BE EF"));
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        QByteArray corruptedFrame = encodeResult.encodedFrame;
        QVERIFY(!corruptedFrame.isEmpty());
        corruptedFrame[0] = static_cast<char>(corruptedFrame.at(0) ^ 0x01);

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);
        QSignalSpy serverDisconnectedSpy(serverSocket, &QTcpSocket::disconnected);

        QCOMPARE(serverSocket->write(corruptedFrame), static_cast<qint64>(corruptedFrame.size()));

        QTRY_COMPARE(protocolErrorSpy.count(), 1);
        QTRY_COMPARE(disconnectedSpy.count(), 1);
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(!networkClient.isConnected());
        QTRY_COMPARE(serverDisconnectedSpy.count(), 1);

        QCOMPARE(frameReceivedSpy.count(), 0);

        const QList<QVariant> errorArguments = protocolErrorSpy.takeFirst();
        QCOMPARE(errorArguments.size(), 2);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::ErrorCode>(errorArguments.at(0)), MiniCloud::Protocol::ErrorCode::InvalidFrame);
        QVERIFY(!errorArguments.at(1).toString().isEmpty());

    }

    void disconnectWithPartialFrame_thenReconnect_doesNotReuseOldBuffer()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        QSignalSpy frameReceivedSpy( &networkClient, &NetworkClient::frameReceived);
        QSignalSpy protocolErrorSpy(&networkClient, &NetworkClient::protocolError);
        QSignalSpy disconnectedSpy( &networkClient, &NetworkClient::disconnected);

        QVERIFY(networkClient.connectToServer( "127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *firstServerSocket = server.nextPendingConnection();
        QVERIFY(firstServerSocket != nullptr);
        QTRY_COMPARE(firstServerSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::FrameEncodeResult firstEncodeResult = MiniCloud::Protocol::serializeFrame(
                MiniCloud::Protocol::MessageType::AuthenticateRequest,
                401,
                3001,
                QByteArray::fromHex("01 02 03 04"));

        QCOMPARE(firstEncodeResult.status,MiniCloud::Protocol::FrameEncodeStatus::Success);

        const QByteArray partialHeader =firstEncodeResult.encodedFrame.left(10);
        QCOMPARE(firstServerSocket->write(partialHeader),static_cast<qint64>(partialHeader.size()));

        QVERIFY(!frameReceivedSpy.wait(100));
        QCOMPARE(frameReceivedSpy.count(), 0);
        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());

        firstServerSocket->disconnectFromHost();
        QTRY_COMPARE(disconnectedSpy.count(), 1);
        QTRY_COMPARE(networkClient.state(),QAbstractSocket::UnconnectedState);
        QVERIFY(!networkClient.isConnected());

        frameReceivedSpy.clear();
        protocolErrorSpy.clear();
        disconnectedSpy.clear();

        QVERIFY(networkClient.connectToServer("127.0.0.1", server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *secondServerSocket = server.nextPendingConnection();
        QVERIFY(secondServerSocket != nullptr);
        QTRY_COMPARE(secondServerSocket->state(),QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::FileChunk;
        const MiniCloud::Protocol::RequestId requestId = 402;
        const MiniCloud::Protocol::TaskId taskId = 3002;
        const QByteArray payload = QByteArray::fromHex("DE AD BE EF 10 20 30 40");

        const MiniCloud::Protocol::FrameEncodeResult secondEncodeResult =MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        QCOMPARE(secondEncodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        QCOMPARE(secondServerSocket->write(secondEncodeResult.encodedFrame), static_cast<qint64>(secondEncodeResult.encodedFrame.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        const QList<QVariant> signalArguments = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 1);

        const MiniCloud::Protocol::ProtocolFrame receivedFrame =qvariant_cast<MiniCloud::Protocol::ProtocolFrame>( signalArguments.at(0));

        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength,  static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QCOMPARE(protocolErrorSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected());
    }
};
QTEST_GUILESS_MAIN(NetworkClientTest)
#include "networkclienttest.moc"
