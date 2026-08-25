#pragma once

#include <QByteArray>
#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QTimer>

#include "protocolframe.h"
#include "errorresponse.h"
#include "requesttypes.h"

class NetworkClient;

class RequestDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit RequestDispatcher(NetworkClient *networkClient, QObject *parent = nullptr);
    RequestDispatcher(NetworkClient *networkClient, qint64 requestTimeoutMs = 5000, QObject *parent = nullptr);

    MiniCloud::Client::RequestSendResult sendRequest(MiniCloud::Protocol::MessageType requestType, MiniCloud::Protocol::TaskId taskId, const QByteArray &payload, MiniCloud::Client::RequestDestination destination);

    qsizetype pendingRequestCount() const;

signals:
    void responseReceived(MiniCloud::Client::RequestDestination destination, const MiniCloud::Protocol::ProtocolFrame &frame);
    void requestFailed(MiniCloud::Client::RequestDestination destination, MiniCloud::Protocol::RequestId requestId, MiniCloud::Protocol::TaskId taskId, MiniCloud::Client::RequestDispatchError errorCode);
    void errorResponseReceived(MiniCloud::Client::RequestDestination destination, MiniCloud::Protocol::RequestId requestId, MiniCloud::Protocol::TaskId taskId, const MiniCloud::Protocol::ErrorResponseData &data);

private slots:
    void onFrameReceived(const MiniCloud::Protocol::ProtocolFrame &frame);
    void onDisconnected();
    void onTimeoutTick();

private:
    struct PendingRequest
    {
        MiniCloud::Protocol::MessageType requestType = MiniCloud::Protocol::MessageType::Invalid;
        MiniCloud::Protocol::MessageType expectedResponseType = MiniCloud::Protocol::MessageType::Invalid;
        MiniCloud::Client::RequestDestination destination = MiniCloud::Client::RequestDestination::Invalid;
        MiniCloud::Protocol::TaskId taskId = 0;
        QDeadlineTimer deadline;
    };

    MiniCloud::Protocol::MessageType expectedResponseTypeFor(MiniCloud::Protocol::MessageType requestType) const;

    NetworkClient *m_networkClient = nullptr;
    QHash<MiniCloud::Protocol::RequestId, PendingRequest> m_pendingRequests;
    MiniCloud::Protocol::RequestId m_nextRequestId = 1;
    QTimer *m_timeoutTimer = nullptr;
    qint64 m_requestTimeoutMs;
};
