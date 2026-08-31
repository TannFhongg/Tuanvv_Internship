#include <QtTest/QTest>
#include "TcpServer.h"
#include <QSignalSpy>
#include "ClientSession.h"
#include "protocolconstants.h"
#include "protocolcodec.h"
#include "frameparser.h"
#include "authentication.h"

class TcpServerTest : public QObject
{
    Q_OBJECT
private slots:

    void startAndStop_updatesListeningState()
    {
        TcpServer server;
        QSignalSpy spy(&server, &TcpServer::listenFailed);

        QVERIFY(server.isListening() == false);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));
        QVERIFY(server.isListening());

        const quint16 port = server.serverPort();
        QVERIFY(port != 0);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0) == false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(server.isListening());
        QCOMPARE(server.serverPort(), port);

        server.stop();
        QVERIFY(server.isListening() == false);
        QCOMPARE(server.serverPort(), 0);
    }

    void firstClientConnection_emitsClientConnected()
    {

        TcpServer server;
        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyRejected(&server, &TcpServer::clientRejected);

        QTcpSocket clientSocket;

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        QVERIFY(server.isListening() == true);
        const quint16 port = server.serverPort();
        QVERIFY(port != 0);

        QVERIFY(spyConnected.isValid() == true);
        QVERIFY(spyRejected.isValid() == true);
        QVERIFY(spyConnected.count() == 0);
        QVERIFY(spyRejected.count() == 0);

        clientSocket.connectToHost(QHostAddress::LocalHost, port);

        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyConnected.count(), 1);
        QCOMPARE(spyRejected.count(), 0);
        QTRY_VERIFY(server.isListening() == true);

        clientSocket.disconnectFromHost();
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);

        server.stop();
        QVERIFY(server.isListening() == false);
    }

    void secondClientConnection_isRejectedWhileFirstRemainsConnected()
    {

        TcpServer server;
        QTcpSocket clientA;
        QTcpSocket clientB;

        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyRejectted(&server, &TcpServer::clientRejected);
        QSignalSpy spyDisconnected(&server, &TcpServer::clientDisconnected);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        const quint16 port = server.serverPort();
        QVERIFY(port != 0);
        clientA.connectToHost(QHostAddress::LocalHost, port);
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QCOMPARE(spyRejectted.count(), 0);

        clientB.connectToHost(QHostAddress::LocalHost, port);
        QTRY_COMPARE(spyRejectted.count(), 1);
        QTRY_COMPARE(clientB.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QVERIFY(spyDisconnected.count() == 0);
        QTRY_VERIFY(server.isListening() == true);

        clientA.disconnectFromHost();
        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        server.stop();
    }

    void newClient_isAcceptedAfterActiveClientDisconnects()
    {
        TcpServer server;
        QTcpSocket clientA;
        QTcpSocket clientC;

        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyDisconnected(&server, &TcpServer::clientDisconnected);
        QSignalSpy spyRejected(&server, &TcpServer::clientRejected);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        clientA.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QCOMPARE(spyRejected.count(), 0);

        clientA.disconnectFromHost();
        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(spyDisconnected.count(), 1);

        clientC.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientC.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 2);
        QTRY_COMPARE(spyDisconnected.count(), 1);
        QCOMPARE(spyRejected.count(), 0);
        QVERIFY(server.isListening() == true);

        clientC.disconnectFromHost();
        QTRY_COMPARE(clientC.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(spyDisconnected.count(), 2);
    }

    void stop_disconnectsActiveClientAndStopsListening()
    {

        TcpServer server;
        QTcpSocket clientA;

        QSignalSpy spyConnected(&server, &TcpServer::clientConnected);
        QSignalSpy spyDisconnected(&server, &TcpServer::clientDisconnected);
        QSignalSpy spyRejected(&server, &TcpServer::clientRejected);

        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        clientA.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientA.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(spyConnected.count(), 1);
        QCOMPARE(spyDisconnected.count(), 0);

        server.stop();
        QVERIFY(server.isListening() == false);
        QCOMPARE(server.serverPort(), 0);

        QTRY_COMPARE(clientA.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(spyDisconnected.count(), 1);
        QVERIFY(server.isListening() == false);

        server.stop();
        QVERIFY(server.isListening() == false);
        QCOMPARE(spyDisconnected.count(), 1);
    }

    void sendFrame_whenDisconnected_returnsFalse()
    {
        auto *socket = new QTcpSocket();
        ClientSession session(socket);

        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        QVERIFY(!session.sendFrame(MiniCloud::Protocol::MessageType::AuthenticateRequest, 1, 0, QByteArray()));

        QCOMPARE(spyFinished.count(), 0);
        QCOMPARE(spyProtocolError.count(), 0);
    }

    void sendFrame_whenConnected_clientReceivesCompleteFrame()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);

        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray::fromHex("12 22 11 22 11");

        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);
        QSignalSpy spyReadyRead(&clientSocket, &QTcpSocket::readyRead);

        QVERIFY(session.sendFrame(messageType, requestId, taskId, payload));
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        const qint64 expectedBytes = static_cast<qint64>(encodeResult.encodedFrame.size());
        QTRY_COMPARE(clientSocket.bytesAvailable(), expectedBytes);
        QTRY_COMPARE(spyReadyRead.count() > 0, true);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);

        QByteArray receivedData = clientSocket.readAll();
        MiniCloud::Protocol::FrameParser parser;
        parser.appendData(receivedData);
        MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();

        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result.frame.header.messageType, messageType);
        QCOMPARE(result.frame.header.requestId, requestId);
        QCOMPARE(result.frame.header.taskId, taskId);
        QCOMPARE(result.frame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(result.frame.payload, payload);
        QCOMPARE(parser.bufferedSize(), 0);

        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);
    }

    void sendFrame_invalidMessageType_returnsFalseAndSendsNoBytes()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::Invalid;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray::fromHex("12 22 11 22 11");

        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);
        QSignalSpy spyReadyRead(&clientSocket, &QTcpSocket::readyRead);

        QVERIFY(!session.sendFrame(messageType, requestId, taskId, payload));
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyReadyRead.count(), 0);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);

        QTRY_COMPARE(clientSocket.bytesAvailable(), 0);
    }

    void sendFrame_payloadTooLarge_returnsFalseAndSendsNoBytes()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::FileChunk;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray(MiniCloud::Protocol::protocolMaxControlPayloadSize + 1, 'A');

        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);
        QSignalSpy spyReadyRead(&clientSocket, &QTcpSocket::readyRead);

        QVERIFY(!session.sendFrame(messageType, requestId, taskId, payload));
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        QTRY_COMPARE(spyReadyRead.count(), 0);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);
        QTRY_COMPARE(clientSocket.bytesAvailable(), 0);
    }

    void sendFrame_twoConsecutiveFrames_clientReceivesBothInOrder()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType_A = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        MiniCloud::Protocol::RequestId requestId_A = 42;
        MiniCloud::Protocol::TaskId taskId_A = 0;
        QByteArray payload_A = QByteArray::fromHex("12 22 11 22 11");

        MiniCloud::Protocol::MessageType messageType_B = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        MiniCloud::Protocol::RequestId requestId_B = 43;
        MiniCloud::Protocol::TaskId taskId_B = 1;
        QByteArray payload_B = QByteArray::fromHex("AA BB CC DD EE");

        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);
        QSignalSpy spyReadyRead(&clientSocket, &QTcpSocket::readyRead);

        QVERIFY(session.sendFrame(messageType_A, requestId_A, taskId_A, payload_A));
        QVERIFY(session.sendFrame(messageType_B, requestId_B, taskId_B, payload_B));

        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyReadyRead.count() > 0, true);
        QTRY_COMPARE(spyFinished.count() == 0, true);
        QTRY_COMPARE(spyProtocolError.count() == 0, true);

        qint64 expectedBytes = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize + payload_A.size() + MiniCloud::Protocol::protocolWireHeaderSize + payload_B.size());
        QTRY_COMPARE(clientSocket.bytesAvailable(), expectedBytes);

        MiniCloud::Protocol::FrameParser parser;
        QByteArray receivedData = clientSocket.readAll();
        parser.appendData(receivedData);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_A = parser.tryTakeFrame();
        QCOMPARE(result_A.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_A.frame.header.messageType, messageType_A);
        QCOMPARE(result_A.frame.header.requestId, requestId_A);
        QCOMPARE(result_A.frame.header.taskId, taskId_A);
        QCOMPARE(result_A.frame.header.payloadLength, static_cast<quint32>(payload_A.size()));
        QCOMPARE(result_A.frame.payload, payload_A);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_B = parser.tryTakeFrame();
        QCOMPARE(result_B.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_B.frame.header.messageType, messageType_B);
        QCOMPARE(result_B.frame.header.requestId, requestId_B);
        QCOMPARE(result_B.frame.header.taskId, taskId_B);
        QCOMPARE(result_B.frame.header.payloadLength, static_cast<quint32>(payload_B.size()));
        QCOMPARE(result_B.frame.payload, payload_B);

        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(parser.bufferedSize(), 0);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_C = parser.tryTakeFrame();
        QCOMPARE(result_C.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);
    }

    void readyRead_completeFrame_emitsFrameReceived()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray::fromHex("12 22 11 22 11");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        const qint64 expectedBytes = static_cast<qint64>(encodeResult.encodedFrame.size());

        QSignalSpy spyFrameReceived(&session, &ClientSession::frameReceived);
        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        clientSocket.write(encodeResult.encodedFrame);

        QTRY_COMPARE(spyFrameReceived.count(), 1);

        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);
        QTRY_COMPARE(clientSocket.bytesAvailable(), 0);

        QList<QVariant> arguments = spyFrameReceived.takeFirst();
        QVERIFY(arguments.size() == 2);

        const MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(arguments.at(1));

        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
    }

    void readyRead_partialHeader_waitsForRemainingBytes()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray::fromHex("12 22 11 22 11");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        const qint64 expectedBytes = static_cast<qint64>(encodeResult.encodedFrame.size());

        QSignalSpy spyFrameReceived(&session, &ClientSession::frameReceived);
        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        constexpr qsizetype partialHeaderSize = 10;
        clientSocket.write(encodeResult.encodedFrame.left(partialHeaderSize));

        QTRY_COMPARE(spyFrameReceived.count(), 0);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);

        QTest::qWait(100);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        clientSocket.write(encodeResult.encodedFrame.mid(partialHeaderSize));
        QTRY_COMPARE(spyFrameReceived.count(), 1);
        QTRY_COMPARE(clientSocket.bytesAvailable(), 0);

        QList<QVariant> arguments = spyFrameReceived.takeFirst();
        QVERIFY(arguments.size() == 2);

        MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(arguments.at(1));
        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QTRY_COMPARE(spyProtocolError.count(), 0);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
    }

    void readyRead_partialPayload_waitsForRemainingBytes()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray::fromHex("12 22 11 22 11");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        const qint64 expectedBytes = static_cast<qint64>(encodeResult.encodedFrame.size());

        QSignalSpy spyFrameReceived(&session, &ClientSession::frameReceived);
        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        const qsizetype splitPosition = static_cast<qsizetype>(MiniCloud::Protocol::protocolWireHeaderSize) + 3;

        QVERIFY(splitPosition < encodeResult.encodedFrame.size());
        clientSocket.write(encodeResult.encodedFrame.left(splitPosition));

        QCOMPARE(spyFrameReceived.count(), 0);
        QCOMPARE(spyFinished.count(), 0);
        QCOMPARE(spyProtocolError.count(), 0);

        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        clientSocket.write(encodeResult.encodedFrame.mid(splitPosition));
        QTRY_COMPARE(spyFrameReceived.count(), 1);

        QList<QVariant> arguments = spyFrameReceived.takeFirst();
        QVERIFY(arguments.size() == 2);

        MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(arguments.at(1));
        QCOMPARE(receivedFrame.header.messageType, messageType);
        QCOMPARE(receivedFrame.header.requestId, requestId);
        QCOMPARE(receivedFrame.header.taskId, taskId);
        QCOMPARE(receivedFrame.header.payloadLength, static_cast<quint32>(payload.size()));
        QCOMPARE(receivedFrame.payload, payload);

        QTRY_COMPARE(spyProtocolError.count(), 0);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
    }

    void readyRead_twoCompleteFrames_emitsBothInOrder()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType_A = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId_A = 42;
        MiniCloud::Protocol::TaskId taskId_A = 0;
        QByteArray payload_A = QByteArray::fromHex("12 22 11 22 11");

        MiniCloud::Protocol::MessageType messageType_B = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId_B = 43;
        MiniCloud::Protocol::TaskId taskId_B = 0;
        QByteArray payload_B = QByteArray::fromHex("AA BB CC DD EE");

        QSignalSpy spyFrameReceived(&session, &ClientSession::frameReceived);
        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        const MiniCloud::Protocol::FrameEncodeResult encodeResult_A = MiniCloud::Protocol::serializeFrame(messageType_A, requestId_A, taskId_A, payload_A);
        const MiniCloud::Protocol::FrameEncodeResult encodeResult_B = MiniCloud::Protocol::serializeFrame(messageType_B, requestId_B, taskId_B, payload_B);
        const qint64 expectedBytes = static_cast<qint64>(encodeResult_A.encodedFrame.size());
        const qint64 expectedBytesB = static_cast<qint64>(encodeResult_B.encodedFrame.size());

        QByteArray combinedEncodedFrames = encodeResult_A.encodedFrame + encodeResult_B.encodedFrame;
        clientSocket.write(combinedEncodedFrames), static_cast<qint64>(combinedEncodedFrames.size());

        QTRY_COMPARE(spyFrameReceived.count(), 2);

        QList<QVariant> arguments_A = spyFrameReceived.takeFirst();
        QVERIFY(arguments_A.size() == 2);
        MiniCloud::Protocol::ProtocolFrame receivedFrame_A = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(arguments_A.at(1));
        QCOMPARE(receivedFrame_A.header.messageType, messageType_A);
        QCOMPARE(receivedFrame_A.header.requestId, requestId_A);
        QCOMPARE(receivedFrame_A.header.taskId, taskId_A);
        QCOMPARE(receivedFrame_A.header.payloadLength, static_cast<quint32>(payload_A.size()));
        QCOMPARE(receivedFrame_A.payload, payload_A);

        QList<QVariant> arguments_B = spyFrameReceived.takeFirst();
        QVERIFY(arguments_B.size() == 2);
        MiniCloud::Protocol::ProtocolFrame receivedFrame_B = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(arguments_B.at(1));
        QCOMPARE(receivedFrame_B.header.messageType, messageType_B);
        QCOMPARE(receivedFrame_B.header.requestId, requestId_B);
        QCOMPARE(receivedFrame_B.header.taskId, taskId_B);
        QCOMPARE(receivedFrame_B.header.payloadLength, static_cast<quint32>(payload_B.size()));
        QCOMPARE(receivedFrame_B.payload, payload_B);

        QTRY_COMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(spyProtocolError.count(), 0);

        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
    }

    void readyRead_completeFrameAndPartialNextFrame_preservesRemainder()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType_A = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId_A = 42;
        MiniCloud::Protocol::TaskId taskId_A = 0;
        QByteArray payload_A = QByteArray::fromHex("12 22 11 22 11");

        MiniCloud::Protocol::MessageType messageType_B = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId_B = 43;
        MiniCloud::Protocol::TaskId taskId_B = 0;
        QByteArray payload_B = QByteArray::fromHex("AA BB CC DD EE");

        QSignalSpy spyFrameReceived(&session, &ClientSession::frameReceived);
        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        const MiniCloud::Protocol::FrameEncodeResult encodeResult_A = MiniCloud::Protocol::serializeFrame(messageType_A, requestId_A, taskId_A, payload_A);
        const MiniCloud::Protocol::FrameEncodeResult encodeResult_B = MiniCloud::Protocol::serializeFrame(messageType_B, requestId_B, taskId_B, payload_B);
        const qint64 expectedBytes = static_cast<qint64>(encodeResult_A.encodedFrame.size());
        const qint64 expectedBytesB = static_cast<qint64>(encodeResult_B.encodedFrame.size());

        constexpr qsizetype prefixSize = 10;
        const QByteArray prefixB = encodeResult_B.encodedFrame.left(prefixSize);
        const QByteArray suffixB = encodeResult_B.encodedFrame.mid(prefixSize);
        const QByteArray firstWrite = encodeResult_A.encodedFrame + prefixB;

        clientSocket.write(firstWrite);
        QTRY_COMPARE(spyFrameReceived.count(), 1);

        const QList<QVariant> signalArguments_A = spyFrameReceived.takeFirst();
        QCOMPARE(signalArguments_A.size(), 2);
        const MiniCloud::Protocol::ProtocolFrame receivedFrame_A = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments_A.at(1));
        QCOMPARE(receivedFrame_A.header.messageType, messageType_A);
        QCOMPARE(receivedFrame_A.header.requestId, requestId_A);
        QCOMPARE(receivedFrame_A.header.taskId, taskId_A);
        QCOMPARE(receivedFrame_A.header.payloadLength, static_cast<quint32>(payload_A.size()));
        QCOMPARE(receivedFrame_A.payload, payload_A);

        QVERIFY(!spyFrameReceived.wait(100));
        QCOMPARE(spyFrameReceived.count(), 0);
        QCOMPARE(spyProtocolError.count(), 0);
        QCOMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        clientSocket.write(suffixB);
        QTRY_COMPARE(spyFrameReceived.count(), 1);
        const QList<QVariant> signalArguments_B = spyFrameReceived.takeFirst();
        QCOMPARE(signalArguments_B.size(), 2);
        const MiniCloud::Protocol::ProtocolFrame receivedFrame_B = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments_B.at(1));
        QCOMPARE(receivedFrame_B.header.messageType, messageType_B);
        QCOMPARE(receivedFrame_B.header.requestId, requestId_B);
        QCOMPARE(receivedFrame_B.header.taskId, taskId_B);
        QCOMPARE(receivedFrame_B.header.payloadLength, static_cast<quint32>(payload_B.size()));
        QCOMPARE(receivedFrame_B.payload, payload_B);

        QCOMPARE(spyProtocolError.count(), 0);
        QCOMPARE(spyFinished.count(), 0);
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
    }

    void readyRead_invalidMagic_emitsProtocolErrorAndFinishesSession()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);
        ClientSession session(serverSocket);

        MiniCloud::Protocol::MessageType messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        MiniCloud::Protocol::RequestId requestId = 42;
        MiniCloud::Protocol::TaskId taskId = 0;
        QByteArray payload = QByteArray::fromHex("12 22 11 22 11");

        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);
        QByteArray invalidFrame = encodeResult.encodedFrame;
        invalidFrame[0] = 0x00;
        clientSocket.write(invalidFrame);

        QSignalSpy spyFrameReceived(&session, &ClientSession::frameReceived);
        QSignalSpy spyFinished(&session, &ClientSession::sessionFinished);
        QSignalSpy spyProtocolError(&session, &ClientSession::protocolError);

        QTRY_COMPARE(spyProtocolError.count(), 1);
        QTRY_COMPARE(spyFinished.count(), 1);
        QTRY_COMPARE(spyFrameReceived.count(), 0);

        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::UnconnectedState);

        const QList<QVariant> signalArguments = spyProtocolError.takeFirst();
        QCOMPARE(signalArguments.size(), 3);

        const MiniCloud::Protocol::ErrorCode errorCode = qvariant_cast<MiniCloud::Protocol::ErrorCode>(signalArguments.at(1));
        QCOMPARE(errorCode, MiniCloud::Protocol::ErrorCode::InvalidFrame);
        QVERIFY(!signalArguments.at(1).toString().isEmpty());
    }

    void newSession_startsUnauthenticated()
    {
        auto *socket = new QTcpSocket();
        ClientSession session(socket);

        QVERIFY(!session.isAuthenticated());
    }

    void acceptedClientFrame_isForwardedByTcpServerWithSessionIdentity()
    {
        TcpServer server;
        QVERIFY(server.startListening(QHostAddress::LocalHost, 0));

        ClientSession *forwardedSession = nullptr;
        MiniCloud::Protocol::ProtocolFrame forwardedFrame;
        int forwardedCount = 0;

        connect(
            &server,
            &TcpServer::frameReceived,
            &server,
            [&](ClientSession *session, const MiniCloud::Protocol::ProtocolFrame &frame)
            {
                forwardedSession = session;
                forwardedFrame = frame;
                ++forwardedCount;
            });

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QTRY_COMPARE(clientSocket.state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::AuthenticateRequestData requestData{
            QStringLiteral("MCLD-1111-2222-3333-4444"),
            QStringLiteral("DEVICE-CLIENT")};

        const MiniCloud::Protocol::AuthenticationEncodeResult requestPayload = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);
        QCOMPARE(requestPayload.status, MiniCloud::Protocol::AuthenticationEncodeResult::Status::Success);

        constexpr MiniCloud::Protocol::RequestId requestId = 51;
        constexpr MiniCloud::Protocol::TaskId taskId = 0;
        const MiniCloud::Protocol::FrameEncodeResult encodedFrame =
            MiniCloud::Protocol::serializeFrame(
                MiniCloud::Protocol::MessageType::AuthenticateRequest,
                requestId,
                taskId,
                requestPayload.payload);
                
        QCOMPARE(encodedFrame.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        QCOMPARE(clientSocket.write(encodedFrame.encodedFrame), static_cast<qint64>(encodedFrame.encodedFrame.size()));
        QVERIFY(clientSocket.waitForBytesWritten());
        QTRY_COMPARE(forwardedCount, 1);

        QVERIFY(forwardedSession != nullptr);
        QCOMPARE(forwardedFrame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(forwardedFrame.header.requestId, requestId);
        QCOMPARE(forwardedFrame.header.taskId, taskId);
        QCOMPARE(forwardedFrame.payload, requestPayload.payload);
    }
};

QTEST_GUILESS_MAIN(TcpServerTest)
#include "tcpservertest.moc"
