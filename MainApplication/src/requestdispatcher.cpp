#include "requestdispatcher.h"
#include "NetworkClient.h"
#include "protocolconstants.h"
namespace
{
    constexpr int timeoutCheckIntervalMs = 100;
    constexpr int defaultRequestTimeoutMs = 5000;

    bool isValidDestination(MiniCloud::Client::RequestDestination destination)
    {
        switch (destination)
        {
        case MiniCloud::Client::RequestDestination::License:
        case MiniCloud::Client::RequestDestination::File:
        case MiniCloud::Client::RequestDestination::Task:
            return true;
        case MiniCloud::Client::RequestDestination::Invalid:
        default:
            return false;
        }
    }
}

RequestDispatcher::RequestDispatcher(NetworkClient *networkClient, QObject *parent)
    : RequestDispatcher(networkClient, defaultRequestTimeoutMs, parent) {}

RequestDispatcher::RequestDispatcher(NetworkClient *networkClient, int requestTimeoutMs, QObject *parent)
    : QObject(parent), m_networkClient(networkClient), m_timeoutTimer(new QTimer(this)), m_requestTimeoutMs(requestTimeoutMs)
{
    Q_ASSERT(m_networkClient);
    Q_ASSERT(m_requestTimeoutMs > 0);

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

    if (!isValidDestination(destination))
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidDestination;
        return result;
    }

    if (payload.size() > MiniCloud::Protocol::protocolMaxFramePayloadSize)
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::PayloadTooLarge;
        return result;
    }

    if (!m_networkClient->isConnected())
    {
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = 0;
        result.errorCode = MiniCloud::Client::RequestDispatchError::NotConnected;
        return result;
    }

    const MiniCloud::Protocol::RequestId requestId = generateRequestId();
    PendingRequest pendingRequest;
    pendingRequest.requestType = requestType;
    pendingRequest.expectedResponseType = expectedResponseType;
    pendingRequest.destination = destination;
    pendingRequest.taskId = taskId;
    pendingRequest.deadline = QDeadlineTimer(m_requestTimeoutMs);

    m_pendingRequests.insert(requestId, pendingRequest);

    if (!m_networkClient->sendFrame(requestType, requestId, taskId, payload))
    {
        m_pendingRequests.remove(requestId);
        result.status = MiniCloud::Client::RequestSendStatus::Failed;
        result.requestId = requestId;

        result.errorCode = m_networkClient->isConnected() ? MiniCloud::Client::RequestDispatchError::SocketWriteFailed : MiniCloud::Client::RequestDispatchError::ConnectionLost;
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
        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(frame.payload);

        if (decodeResult.status != MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Success)
        {
            const MiniCloud::Client::RequestDestination destination = pendingRequestIt->destination;
            const MiniCloud::Protocol::TaskId taskId = pendingRequestIt->taskId;
            m_pendingRequests.erase(pendingRequestIt);

            if (m_pendingRequests.isEmpty())
            {
                m_timeoutTimer->stop();
            }

            emit requestFailed(destination, frame.header.requestId, taskId, MiniCloud::Client::RequestDispatchError::InvalidResponsePayload);
            return;
        }

        const MiniCloud::Client::RequestDestination destination = pendingRequestIt->destination;
        const MiniCloud::Protocol::TaskId taskId = pendingRequestIt->taskId;
        m_pendingRequests.erase(pendingRequestIt);

        if (m_pendingRequests.isEmpty())
        {
            m_timeoutTimer->stop();
        }

        emit errorResponseReceived(destination, frame.header.requestId, taskId, decodeResult.data);
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
    m_timeoutTimer->stop();

    QList<QPair<MiniCloud::Protocol::RequestId, PendingRequest>> pendingRequests;
    pendingRequests.reserve(m_pendingRequests.size());

    for (auto it = m_pendingRequests.cbegin(); it != m_pendingRequests.cend(); ++it)
    {
        pendingRequests.append(qMakePair(it.key(), it.value()));
    }
    m_pendingRequests.clear();

    for (const auto &[requestId, pendingRequest] : pendingRequests)
    {
        emit requestFailed(pendingRequest.destination, requestId, pendingRequest.taskId, MiniCloud::Client::RequestDispatchError::ConnectionLost);
    }
}

void RequestDispatcher::onTimeoutTick()
{
    QList<QPair<MiniCloud::Protocol::RequestId, PendingRequest>> expiredRequests;

    for (auto it = m_pendingRequests.cbegin(); it != m_pendingRequests.cend(); ++it)
    {
        if (it->deadline.hasExpired())
        {
            expiredRequests.append(qMakePair(it.key(), it.value()));
        }
    }

    for (const auto &expiredRequest : expiredRequests)
    {
        m_pendingRequests.remove(expiredRequest.first);
    }

    if (m_pendingRequests.isEmpty())
    {
        m_timeoutTimer->stop();
    }

    for (const auto &expiredRequest : expiredRequests)
    {
        const MiniCloud::Protocol::RequestId requestId = expiredRequest.first;
        const PendingRequest &pendingRequest = expiredRequest.second;

        emit requestFailed(
            pendingRequest.destination,
            requestId,
            pendingRequest.taskId,
            MiniCloud::Client::RequestDispatchError::RequestTimeout);
    }
}
MiniCloud::Protocol::RequestId RequestDispatcher::generateRequestId()
{
    while (true)
    {
        const MiniCloud::Protocol::RequestId requestId = m_nextRequestId++;

        if (requestId == 0)
            continue;

        if (m_pendingRequests.contains(requestId))
        {
            continue;
        }
        return requestId;
    }
}