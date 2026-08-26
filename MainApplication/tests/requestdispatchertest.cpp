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
#include "errorresponse.h"

class RequestDispatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<MiniCloud::Client::RequestDestination>("MiniCloud::Client::RequestDestination");
        qRegisterMetaType<MiniCloud::Client::RequestDispatchError>("MiniCloud::Client::RequestDispatchError");
        qRegisterMetaType<MiniCloud::Protocol::ErrorResponseData>("MiniCloud::Protocol::ErrorResponseData");
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

    void errorResponse_matchingRequest_removesPendingAndEmitsRemoteError()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());
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

        MiniCloud::Protocol::ErrorResponseData errorData;
        errorData.errorCode = MiniCloud::Protocol::ErrorCode::AuthenticationFailed;
        errorData.message = QStringLiteral("Authentication failed");
        errorData.details.insert(QStringLiteral("reason"), QStringLiteral("Invalid credentials"));

        const MiniCloud::Protocol::ErrorResponseEncodeResult encodeResult = MiniCloud::Protocol::serializeErrorResponse(errorData);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Success);
        QVERIFY(!encodeResult.payload.isEmpty());

        const MiniCloud::Protocol::FrameEncodeResult frameEncodeResult = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::ErrorResponse, requestFrameResult.frame.header.requestId, 0, encodeResult.payload);
        QCOMPARE(frameEncodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes = serverSocket->write(frameEncodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(frameEncodeResult.encodedFrame.size()));

        QTRY_COMPARE(errorResponseSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(requestFailedSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const QList<QVariant> signalArguments = errorResponseSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 4);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDestination>(signalArguments.at(0)), MiniCloud::Client::RequestDestination::License);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::RequestId>(signalArguments.at(1)), requestResult.requestId);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::TaskId>(signalArguments.at(2)), requestTaskId);
        const MiniCloud::Protocol::ErrorResponseData receivedError = qvariant_cast<MiniCloud::Protocol::ErrorResponseData>(signalArguments.at(3));
        QCOMPARE(receivedError.errorCode, errorData.errorCode);
        QCOMPARE(receivedError.message, errorData.message);
        QCOMPARE(receivedError.details, errorData.details);
    }

    void errorResponse_malformedPayload_removesPendingAndEmitsInvalidResponsePayload()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);
        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());
        QSignalSpy disconnectedSpy(&networkClient, &NetworkClient::disconnected);
        QVERIFY(disconnectedSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::TaskId requestTaskId = 0;
        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");
        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest, requestTaskId, requestPayload, MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);

        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());

        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult.frame.header.requestId, requestResult.requestId);

        QByteArray malformedPayload = QByteArrayLiteral("{\"errorCode\":");
        const MiniCloud::Protocol::FrameEncodeResult frameEncodeResult =
            MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::ErrorResponse, requestFrameResult.frame.header.requestId, 0, malformedPayload);

        QCOMPARE(frameEncodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);

        const qint64 queuedBytes = serverSocket->write(frameEncodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(frameEncodeResult.encodedFrame.size()));

        QTRY_COMPARE(requestFailedSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        const QList<QVariant> signalArguments = requestFailedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 4);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDestination>(signalArguments.at(0)),
                 MiniCloud::Client::RequestDestination::License);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::RequestId>(signalArguments.at(1)),
                 requestResult.requestId);
        QCOMPARE(qvariant_cast<MiniCloud::Protocol::TaskId>(signalArguments.at(2)),
                 requestTaskId);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDispatchError>(signalArguments.at(3)),
                 MiniCloud::Client::RequestDispatchError::InvalidResponsePayload);
    }

    void disconnect_withPendingRequests_clearsAllAndEmitsConnectionLostForEach()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);

        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());

        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());

        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const QByteArray firstPayload = QByteArrayLiteral("authenticate request 1");
        const QByteArray secondPayload = QByteArrayLiteral("authenticate request 2");

        const MiniCloud::Client::RequestSendResult firstRequest = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            0,
            firstPayload,
            MiniCloud::Client::RequestDestination::License);

        const MiniCloud::Client::RequestSendResult secondRequest = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            0,
            secondPayload,
            MiniCloud::Client::RequestDestination::License);

        QCOMPARE(firstRequest.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(secondRequest.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QVERIFY(firstRequest.requestId != secondRequest.requestId);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(2));

        serverSocket->abort();

        QTRY_COMPARE(networkClient.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(requestFailedSpy.count(), 2);

        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));

        QList<MiniCloud::Protocol::RequestId> receivedRequestIds;
        for (const QList<QVariant> &arguments : requestFailedSpy)
        {
            QCOMPARE(arguments.size(), 4);
            QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDestination>(arguments.at(0)), MiniCloud::Client::RequestDestination::License);
            const MiniCloud::Protocol::RequestId requestId = qvariant_cast<MiniCloud::Protocol::RequestId>(arguments.at(1));
            QCOMPARE(
                qvariant_cast<MiniCloud::Protocol::TaskId>(arguments.at(2)),
                MiniCloud::Protocol::TaskId(0));
            QCOMPARE(
                qvariant_cast<MiniCloud::Client::RequestDispatchError>(arguments.at(3)),
                MiniCloud::Client::RequestDispatchError::ConnectionLost);
            receivedRequestIds.append(requestId);
        }

        QCOMPARE(receivedRequestIds.count(firstRequest.requestId), 1);
        QCOMPARE(receivedRequestIds.count(secondRequest.requestId), 1);
    }

    void pendingRequest_whenDeadlineExpires_emitsRequestTimeout()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient, 50);

        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());
        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::TaskId requestTaskId = 0;
        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");

        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            requestTaskId,
            requestPayload,
            MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        QTRY_COMPARE_WITH_TIMEOUT(requestFailedSpy.count(), 1, 1000);
        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        const QList<QVariant> signalArguments = requestFailedSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 4);

        QCOMPARE(
            qvariant_cast<MiniCloud::Client::RequestDestination>(signalArguments.at(0)),
            MiniCloud::Client::RequestDestination::License);

        QCOMPARE(
            qvariant_cast<MiniCloud::Protocol::RequestId>(signalArguments.at(1)),
            requestResult.requestId);

        QCOMPARE(
            qvariant_cast<MiniCloud::Protocol::TaskId>(signalArguments.at(2)),
            requestTaskId);

        QCOMPARE(
            qvariant_cast<MiniCloud::Client::RequestDispatchError>(signalArguments.at(3)),
            MiniCloud::Client::RequestDispatchError::RequestTimeout);
    }

    void response_afterTimeout_isIgnoredAndDoesNotCompleteTwice()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient, 50);

        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());
        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QVERIFY(frameReceivedSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::TaskId requestTaskId = 0;
        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");

        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            requestTaskId,
            requestPayload,
            MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);

        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());

        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);

        QCOMPARE(requestFrameResult.frame.header.requestId, requestResult.requestId);

        QCOMPARE(requestFrameResult.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);

        QTRY_COMPARE_WITH_TIMEOUT(requestFailedSpy.count(), 1, 1000);

        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        const QList<QVariant> timeoutArguments = requestFailedSpy.first();
        QCOMPARE(timeoutArguments.size(), 4);
        QCOMPARE(
            qvariant_cast<MiniCloud::Client::RequestDestination>(timeoutArguments.at(0)),
            MiniCloud::Client::RequestDestination::License);
        QCOMPARE(
            qvariant_cast<MiniCloud::Protocol::RequestId>(timeoutArguments.at(1)),
            requestResult.requestId);
        QCOMPARE(
            qvariant_cast<MiniCloud::Protocol::TaskId>(timeoutArguments.at(2)),
            requestTaskId);
        QCOMPARE(
            qvariant_cast<MiniCloud::Client::RequestDispatchError>(timeoutArguments.at(3)),
            MiniCloud::Client::RequestDispatchError::RequestTimeout);

        const QByteArray responsePayload = QByteArrayLiteral("late response");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::AuthenticateResponse, requestFrameResult.frame.header.requestId, 0, responsePayload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes = serverSocket->write(encodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(encodeResult.encodedFrame.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);

        QTRY_COMPARE(requestFailedSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));

        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);
    }

    void response_beforeDeadline_doesNotEmitTimeoutLater()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient, 300);

        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());
        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QVERIFY(frameReceivedSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::TaskId requestTaskId = 0;
        const QByteArray requestPayload = QByteArrayLiteral("authenticate request");
        const MiniCloud::Client::RequestSendResult requestResult = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            requestTaskId,
            requestPayload,
            MiniCloud::Client::RequestDestination::License);

        QCOMPARE(requestResult.status, MiniCloud::Client::RequestSendStatus::Accepted);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);
        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());

        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult.frame.header.requestId, requestResult.requestId);
        QCOMPARE(requestFrameResult.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);

        QCOMPARE(responseSpy.count(), 0);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(1));

        const QByteArray responsePayload = QByteArrayLiteral("authenticate response");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::AuthenticateResponse, requestFrameResult.frame.header.requestId, 0, responsePayload);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes = serverSocket->write(encodeResult.encodedFrame);
        QCOMPARE(queuedBytes, static_cast<qint64>(encodeResult.encodedFrame.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 1);

        QTRY_COMPARE_WITH_TIMEOUT(requestFailedSpy.count(), 0, 500);

        QTRY_COMPARE(frameReceivedSpy.count(), 1);
        QCOMPARE(responseSpy.count(), 1);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));
        QCOMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        const QList<QVariant> signalArguments = responseSpy.takeFirst();
        QCOMPARE(signalArguments.size(), 2);
        QCOMPARE(qvariant_cast<MiniCloud::Client::RequestDestination>(signalArguments.at(0)), MiniCloud::Client::RequestDestination::License);
        const MiniCloud::Protocol::ProtocolFrame responseFrame = qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(1));
        QCOMPARE(responseFrame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateResponse);
        QCOMPARE(responseFrame.header.requestId, requestResult.requestId);
        QCOMPARE(responseFrame.payload, responsePayload);
    }

    void responses_outOfOrder_areCorrelatedByRequestId()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        NetworkClient networkClient;
        RequestDispatcher dispatcher(&networkClient);

        QSignalSpy requestFailedSpy(&dispatcher, &RequestDispatcher::requestFailed);
        QVERIFY(requestFailedSpy.isValid());
        QSignalSpy responseSpy(&dispatcher, &RequestDispatcher::responseReceived);
        QVERIFY(responseSpy.isValid());
        QSignalSpy errorResponseSpy(&dispatcher, &RequestDispatcher::errorResponseReceived);
        QVERIFY(errorResponseSpy.isValid());
        QSignalSpy frameReceivedSpy(&networkClient, &NetworkClient::frameReceived);
        QVERIFY(frameReceivedSpy.isValid());

        QVERIFY(networkClient.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort()));
        QTRY_COMPARE(networkClient.state(), QAbstractSocket::ConnectedState);

        QTRY_VERIFY(server.hasPendingConnections());
        QTcpSocket *serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket != nullptr);
        QTRY_COMPARE(serverSocket->state(), QAbstractSocket::ConnectedState);

        const MiniCloud::Protocol::TaskId requestTaskId_A = 0;
        const QByteArray requestPayload_A = QByteArrayLiteral("authenticate request");

        const MiniCloud::Client::RequestSendResult requestResult_A = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            requestTaskId_A,
            requestPayload_A,
            MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult_A.status, MiniCloud::Client::RequestSendStatus::Accepted);

        const MiniCloud::Protocol::TaskId requestTaskId_B = 1;
        const QByteArray requestPayload_B = QByteArrayLiteral("authenticate request");

        const MiniCloud::Client::RequestSendResult requestResult_B = dispatcher.sendRequest(
            MiniCloud::Protocol::MessageType::AuthenticateRequest,
            requestTaskId_B,
            requestPayload_B,
            MiniCloud::Client::RequestDestination::License);
        QCOMPARE(requestResult_B.status, MiniCloud::Client::RequestSendStatus::Accepted);

        const qint64 requestByteCount = static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload_A.size() +
                                        static_cast<qint64>(MiniCloud::Protocol::protocolWireHeaderSize) + requestPayload_B.size();
        QTRY_COMPARE(serverSocket->bytesAvailable(), requestByteCount);

        MiniCloud::Protocol::FrameParser requestParser;
        requestParser.appendData(serverSocket->readAll());

        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult_A = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult_A.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult_A.frame.header.requestId, requestResult_A.requestId);
        const MiniCloud::Protocol::FrameParser::FrameParseResult requestFrameResult_B = requestParser.tryTakeFrame();
        QCOMPARE(requestFrameResult_B.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(requestFrameResult_B.frame.header.requestId, requestResult_B.requestId);

        QCOMPARE(requestFrameResult_A.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(requestFrameResult_B.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);

        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(2));

        const QByteArray responsePayload_B = QByteArrayLiteral("response-2");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult_B = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::AuthenticateResponse, requestFrameResult_B.frame.header.requestId, 0, responsePayload_B);
        QCOMPARE(encodeResult_B.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes_B = serverSocket->write(encodeResult_B.encodedFrame);
        QCOMPARE(queuedBytes_B, static_cast<qint64>(encodeResult_B.encodedFrame.size()));
        const QByteArray responsePayload_A = QByteArrayLiteral("response-1");
        const MiniCloud::Protocol::FrameEncodeResult encodeResult_A = MiniCloud::Protocol::serializeFrame(MiniCloud::Protocol::MessageType::AuthenticateResponse, requestFrameResult_A.frame.header.requestId, 0, responsePayload_A);
        QCOMPARE(encodeResult_A.status, MiniCloud::Protocol::FrameEncodeStatus::Success);
        const qint64 queuedBytes_A = serverSocket->write(encodeResult_A.encodedFrame);
        QCOMPARE(queuedBytes_A, static_cast<qint64>(encodeResult_A.encodedFrame.size()));

        const qint64 totalResponseBytes = static_cast<qint64>(encodeResult_A.encodedFrame.size()) + static_cast<qint64>(encodeResult_B.encodedFrame.size());
        QTRY_COMPARE(serverSocket->bytesToWrite(), totalResponseBytes);
        QCOMPARE(totalResponseBytes, static_cast<qint64>(encodeResult_A.encodedFrame.size()) + static_cast<qint64>(encodeResult_B.encodedFrame.size()));
        QTRY_COMPARE(frameReceivedSpy.count(), 2);
        QCOMPARE(responseSpy.count(), 2);
        QCOMPARE(errorResponseSpy.count(), 0);
        QCOMPARE(dispatcher.pendingRequestCount(), qsizetype(0));

        const auto verifyResponse = [&](const MiniCloud::Client::RequestSendResult &requestResult, const QByteArray &expectedPayload)
        {
            const QList<QVariant> signalArguments = responseSpy.takeFirst();
            QCOMPARE(signalArguments.size(), 2);

            QCOMPARE(
                qvariant_cast<MiniCloud::Client::RequestDestination>(signalArguments.at(0)),
                MiniCloud::Client::RequestDestination::License);

            const MiniCloud::Protocol::ProtocolFrame responseFrame =
                qvariant_cast<MiniCloud::Protocol::ProtocolFrame>(signalArguments.at(1));

            QCOMPARE(responseFrame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateResponse);
            QCOMPARE(responseFrame.header.requestId, requestResult.requestId);
            QCOMPARE(responseFrame.payload, expectedPayload);
        };

        verifyResponse(requestResult_B, responsePayload_B);
        verifyResponse(requestResult_A, responsePayload_A);
    }
};

QTEST_MAIN(RequestDispatcherTest)
#include "requestdispatchertest.moc"
