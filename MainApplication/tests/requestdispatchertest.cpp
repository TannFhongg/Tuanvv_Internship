#include <QtTest/QTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include "NetworkClient.h"
#include "requestdispatcher.h"
#include "protocolframe.h"
#include "frameparser.h"
#include "protocolconstants.h"
#include "protocolcodec.h"
class RequestDispatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<MiniCloud::Client::RequestDestination>("MiniCloud::Client::RequestDestination");
        qRegisterMetaType<MiniCloud::Client::RequestDispatchError>("MiniCloud::Client::RequestDispatchError");
        qRegisterMetaType<MiniCloud::Protocol::ProtocolFrame>("MiniCloud::Protocol::ProtocolFrame");
    }

    void sendRequest_invalidMessageType_returnsFailedWithoutPending()
    {
        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);

        QCOMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        const MiniCloud::Client::RequestSendResult result = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateResponse, 0, QByteArray(), MiniCloud::Client::RequestDestination::License);

        QCOMPARE(result.status, MiniCloud::Client::RequestSendStatus::Failed);
        QCOMPARE(result.requestId, MiniCloud::Protocol::RequestId(0));
        QCOMPARE(result.errorCode, MiniCloud::Client::RequestDispatchError::InvalidMessageType);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
    }

    void sendRequest_invalidDestination_returnsFailedWithoutPending()
    {
        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);

        QCOMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        const MiniCloud::Client::RequestSendResult result = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, QByteArray(), MiniCloud::Client::RequestDestination::Invalid);

        QCOMPARE(result.status, MiniCloud::Client::RequestSendStatus::Failed);
        QCOMPARE(result.requestId, MiniCloud::Protocol::RequestId(0));
        QCOMPARE(result.errorCode, MiniCloud::Client::RequestDispatchError::InvalidDestination);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
    }

    void sendRequest_whenDisconnected_returnsNotConnectedWithoutPending()
    {
        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);

        QCOMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        const MiniCloud::Client::RequestSendResult result = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, QByteArray(), MiniCloud::Client::RequestDestination::License);

        QCOMPARE(result.status, MiniCloud::Client::RequestSendStatus::Failed);
        QCOMPARE(result.requestId, MiniCloud::Protocol::RequestId(0));
        QCOMPARE(result.errorCode, MiniCloud::Client::RequestDispatchError::NotConnected);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QVERIFY(networkClient.isConnected() == false);
    }

    void sendRequest_whenConnected_returnsAcceptedAndCreatesPending()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QVERIFY(networkClient.isConnected() == true);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Client::RequestSendResult result = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, QByteArray(), MiniCloud::Client::RequestDestination::License);

        QCOMPARE(result.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(result.requestId, MiniCloud::Protocol::RequestId(1));
        QCOMPARE(result.errorCode, MiniCloud::Client::RequestDispatchError::None);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        MiniCloud::Protocol::FrameParser parser;
        QTRY_COMPARE(serverSocket->bytesAvailable(), MiniCloud::Protocol::protocolWireHeaderSize);
        QByteArray receivedData = serverSocket->readAll();
        parser.appendData(receivedData);

        MiniCloud::Protocol::FrameParser::FrameParseResult parseResult = parser.tryTakeFrame();
        QCOMPARE(parseResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parseResult.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(parseResult.frame.header.requestId, MiniCloud::Protocol::RequestId(1));
        QCOMPARE(parseResult.frame.header.taskId, MiniCloud::Protocol::TaskId(0));
        QCOMPARE(parseResult.frame.header.payloadLength, static_cast<quint32>(0));
        QCOMPARE(parseResult.frame.payload, QByteArray());
    }

    void sendRequest_twoRequests_generatesDistinctIdsAndTracksBoth()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const QByteArray firstPayload = QByteArrayLiteral("first payload");
        const QByteArray secondPayload = QByteArrayLiteral("second payload");
        const MiniCloud::Client::RequestSendResult firstResult = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, firstPayload, MiniCloud::Client::RequestDestination::License);
        const MiniCloud::Client::RequestSendResult secondResult = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, secondPayload, MiniCloud::Client::RequestDestination::License);

        QCOMPARE(firstResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(secondResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(firstResult.errorCode, MiniCloud::Client::RequestDispatchError::None);
        QCOMPARE(secondResult.errorCode, MiniCloud::Client::RequestDispatchError::None);
        QVERIFY(firstResult.requestId != 0);
        QVERIFY(secondResult.requestId != 0);
        QVERIFY(firstResult.requestId != secondResult.requestId);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(2));

        const qint64 expectedByteCount = static_cast<qint64>(2 * MiniCloud::Protocol::protocolWireHeaderSize) + firstPayload.size() + secondPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), expectedByteCount);

        MiniCloud::Protocol::FrameParser parser;
        parser.appendData(serverSocket->readAll());

        const MiniCloud::Protocol::FrameParser::FrameParseResult firstFrameResult = parser.tryTakeFrame();
        const MiniCloud::Protocol::FrameParser::FrameParseResult secondFrameResult = parser.tryTakeFrame();

        QCOMPARE(firstFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(secondFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(firstFrameResult.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(secondFrameResult.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(firstFrameResult.frame.header.requestId, firstResult.requestId);
        QCOMPARE(secondFrameResult.frame.header.requestId, secondResult.requestId);
        QCOMPARE(firstFrameResult.frame.payload, firstPayload);
        QCOMPARE(secondFrameResult.frame.payload, secondPayload);
        QCOMPARE(parser.bufferedSize(), qsizetype(0));
    }

    void response_matchingRequest_removesPendingAndEmitsResponse()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");
        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, requestPayload, MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);
        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());
        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult.frame.header.requestId, requestResult.requestId);

        const QByteArray responsePayload = QByteArrayLiteral("authenticate response");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::AuthenticateResponse, requestFrameResult.frame.header.requestId, 0, responsePayload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes = serverSocket->write(encodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(encodeResult.encodedFrame.size()));

        QTRY_COMPARE(responseSpy.count(), 1);
        const QList<QVariant> signalArguments = responseSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 2);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDestination>(signalArguments.at(0)), MiniCloud::Client::RequestDestination::License);
        const MiniCloud::Protocol::ProtocolFrame responseFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(1));
        QCOMPARE(responseFrame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestResult.requestId);
        QCOMPARE(responseFrame.payload, responsePayload);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
    }

    void response_unknownRequestId_isIgnoredAndKeepsPending()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");
        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, 0, requestPayload, MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);

        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());
        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult.frame.header.requestId, requestResult.requestId);

        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QVERIFY(frameReceivedSpy.isValid());

        const QByteArray responsePayload = QByteArrayLiteral("authenticate response");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::AuthenticateResponse, requestFrameResult.frame.header.requestId + 1000, 0, responsePayload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes = serverSocket->write(encodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(encodeResult.encodedFrame.size()));

        QTRY_COMPARE(frameReceivedSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 0);

        const QList<QVariant> signalArguments = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 1);
        const MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(0));
        QCOMPARE(receivedFrame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateResponse);
        QCOMPARE(receivedFrame.header.requestId, requestResult.requestId + 1000);
        QCOMPARE(receivedFrame.payload, responsePayload);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
    }

    void response_wrongMessageType_removesPendingAndEmitsResponseTypeMismatch()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QVERIFY(frameReceivedSpy.isValid());
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);
        QVERIFY(disconnectedSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::TaskId requestTaskId = 42;
        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");
        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest, requestTaskId, requestPayload, MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);

        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());
        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult.frame.header.requestId, requestResult.requestId);

        const QByteArray responsePayload = QByteArrayLiteral("FileChunk response");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::FileChunk, requestFrameResult.frame.header.requestId, 999, responsePayload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes = serverSocket->write(encodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(encodeResult.encodedFrame.size()));

        QTRY_COMPARE(frameReceivedSpy.count(), 1);
        QTRY_COMPARE(requestFailedSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const QList<QVariant> failureArguments = requestFailedSpy.takeFirst();
        QCOMPARE(failureArguments.size(), 4);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDestination>(failureArguments.at(0)), MiniCloud::Client::RequestDestination::License);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::RequestId>(failureArguments.at(1)), requestResult.requestId);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::TaskId>(failureArguments.at(2)), requestTaskId);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDispatchError>(failureArguments.at(3)), MiniCloud::Client::RequestDispatchError::ResponseTypeMismatch);

        const QList<QVariant> signalArguments = frameReceivedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 1);
        const MiniCloud::Protocol::ProtocolFrame receivedFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(0));
        QCOMPARE(receivedFrame.header.messageType, MiniCloud::Protocol::MessageType::FileChunk);
        QCOMPARE(receivedFrame.header.requestId, requestResult.requestId);
        QCOMPARE(receivedFrame.payload, responsePayload);
    }
};

QTEST_MAIN(RequestDispatcherTest)
#include "requestdispatchertest.moc"
