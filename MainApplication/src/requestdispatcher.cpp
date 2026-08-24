#include "requestdispatcher.h"

#include "NetworkClient.h"

namespace
{
constexpr int timeoutCheckIntervalMs = 100;
}

RequestDispatcher::RequestDispatcher(NetworkClient *networkClient, QObject *parent)
    : QObject(parent), m_networkClient(networkClient), m_timeoutTimer(new QTimer(this))
{
    Q_ASSERT(m_networkClient);

    m_timeoutTimer->setInterval(timeoutCheckIntervalMs);

    connect(m_networkClient, &NetworkClient::frameReceived, this, &RequestDispatcher::onFrameReceived);
    connect(m_networkClient, &NetworkClient::disconnected, this, &RequestDispatcher::onDisconnected);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RequestDispatcher::onTimeoutTick);

}

qsizetype RequestDispatcher::pendingRequestCount() const
{
    return m_pendingRequests.size();
}

MiniCloud::Protocol::MessageType RequestDispatcher::expectedResponseTypeFor(MiniCloud::Protocol::MessageType requestType) const
{
    if (requestType == MiniCloud::Protocol::MessageType::AuthenticateRequest)
    {
        return MiniCloud::Protocol::MessageType::AuthenticateResponse;
    }

    return MiniCloud::Protocol::MessageType::Invalid;
}

MiniCloud::Client::RequestSendResult RequestDispatcher::sendRequest(MiniCloud::Protocol::MessageType requestType, MiniCloud::Protocol::TaskId taskId, const QByteArray &payload, MiniCloud::Client::RequestDestination destination)
{
    Q_UNUSED(taskId);
    Q_UNUSED(payload);
    Q_UNUSED(destination);

    MiniCloud::Client::RequestSendResult result;

    if (expectedResponseTypeFor(requestType) == MiniCloud::Protocol::MessageType::Invalid)
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidMessageType;
        return result;
    }

    return result;
}

void RequestDispatcher::onFrameReceived(const MiniCloud::Protocol::ProtocolFrame &frame)
{
    
}

void RequestDispatcher::onDisconnected()
{
}

void RequestDispatcher::onTimeoutTick()
{
}
