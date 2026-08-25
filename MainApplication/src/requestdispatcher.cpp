#include "requestdispatcher.h"

#include "NetworkClient.h"

namespace
{
    constexpr int timeoutCheckIntervalMs = 100;
    constexpr qint64 requestTimeoutMs = 5000;
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
    MiniCloud::Client::RequestSendResult result;
    const MiniCloud::Protocol::MessageType expectedResponseType = expectedResponseTypeFor(requestType);

    if (expectedResponseType == MiniCloud::Protocol::MessageType::Invalid)
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidMessageType;
        return result;
    }

    if (destination == MiniCloud::Client::RequestDestination::Invalid)
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidDestination;
        return result;
    }

    if (!m_networkClient->isConnected())
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::NotConnected;
        return result;
    }

    const MiniCloud::Protocol::RequestId requestId = m_nextRequestId++;
    PendingRequest pendingRequest;
    pendingRequest.requestType = requestType;
    pendingRequest.expectedResponseType = expectedResponseType;
    pendingRequest.destination = destination;
    pendingRequest.taskId = taskId;
    pendingRequest.deadline = QDeadlineTimer(requestTimeoutMs);

    m_pendingRequests.insert(requestId, pendingRequest);

    if (!m_networkClient->sendFrame(requestType, requestId, taskId, payload))
    {
        m_pendingRequests.remove(requestId);
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::SocketWriteFailed;
        return result;
    }

    if (!m_timeoutTimer->isActive())
    {
        m_timeoutTimer->start();
    }

    result.status = MiniCloud::Client::RequestSendStatus::Accepted;
    result.requestId = requestId;
    result.errorCode = MiniCloud::Client::RequestDispatchError::None;

    return result;
}

void RequestDispatcher::onFrameReceived(const MiniCloud::Protocol::ProtocolFrame &frame)
{
    const auto pendingRequestIt = m_pendingRequests.constFind(frame.header.requestId);

    if (pendingRequestIt == m_pendingRequests.cend())
    {
        return;
    }

    if (frame.header.messageType == pendingRequestIt->expectedResponseType)
    {
        const MiniCloud::Client::RequestDestination destination = pendingRequestIt->destination;
        m_pendingRequests.erase(pendingRequestIt);

        if (m_pendingRequests.isEmpty())
        {
            m_timeoutTimer->stop();
        }

        emit responseReceived(destination, frame);
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::ErrorResponse)
    {
        return;
    }

    const MiniCloud::Client::RequestDestination destination = pendingRequestIt->destination;
    const MiniCloud::Protocol::TaskId taskId = pendingRequestIt->taskId;
    m_pendingRequests.erase(pendingRequestIt);

    if (m_pendingRequests.isEmpty())
    {
        m_timeoutTimer->stop();
    }

    emit requestFailed(destination, frame.header.requestId, taskId, MiniCloud::Client::RequestDispatchError::ResponseTypeMismatch);
}

void RequestDispatcher::onDisconnected()
{
}

void RequestDispatcher::onTimeoutTick()
{
}
