#include "applicationcontroller.h"

#include "authentication.h"
#include "requesttypes.h"

ApplicationController::ApplicationController(QObject *parent)
    : ApplicationController(5000, parent) {}

ApplicationController::ApplicationController(int requestTimeoutMs, QObject *parent)
    : QObject(parent), m_networkClient(), m_requestDispatcher(&m_networkClient, requestTimeoutMs)
{
    connect(
        &m_requestDispatcher,
        &RequestDispatcher::responseReceived,
        this,
        &ApplicationController::onResponseReceived);
        
    connect(
        &m_requestDispatcher,
        &RequestDispatcher::errorResponseReceived,
        this,
        &ApplicationController::onErrorResponseReceived);
        
    connect(
        &m_requestDispatcher,
        &RequestDispatcher::requestFailed,
        this,
        &ApplicationController::onRequestFailed);
        
    connect(
        &m_networkClient,
        &NetworkClient::disconnected,
        this,
        &ApplicationController::onDisconnected);
        
    connect(
        &m_networkClient,
        &NetworkClient::connected,
        this,
        [this]()
        {
            emit connectionStateChanged(true);
        });
}

MiniCloud::Client::ClientAccessState ApplicationController::accessState() const noexcept
{
    return m_accessState;
}

bool ApplicationController::isFeatureAccessAllowed() const noexcept
{
    return m_accessState == MiniCloud::Client::ClientAccessState::Active;
}

bool ApplicationController::connectToServer(const QString &hostname, quint16 port)
{
    return m_networkClient.connectToServer(hostname, port);
}

void ApplicationController::disconnectFromServer()
{
    m_networkClient.disconnectFromServer();
}

bool ApplicationController::isConnected() const
{
    return m_networkClient.isConnected();
}

MiniCloud::Client::RequestSendResult ApplicationController::activate(const QString &licenseKey, const QString &deviceId)
{
    if (m_accessState == MiniCloud::Client::ClientAccessState::Authenticating)
    {
        MiniCloud::Client::RequestSendResult result;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidRequest;
        return result;
    }

    if (licenseKey.trimmed().isEmpty() || deviceId.trimmed().isEmpty())
    {
        MiniCloud::Client::RequestSendResult result;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidRequest;
        return result;
    }

    const MiniCloud::Protocol::AuthenticateRequestData requestData{licenseKey, deviceId};

    const auto encoded = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);

    if (encoded.status != MiniCloud::Protocol::AuthenticationEncodeResult::Status::Success)
    {
        m_activationRequestId = 0;
        setAccessState(MiniCloud::Client::ClientAccessState::Locked);
        MiniCloud::Client::RequestSendResult result;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidRequest;
        return result;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest,
                                                                                        0, // task id
                                                                                        encoded.payload,
                                                                                        MiniCloud::Client::RequestDestination::License);

    if (result.status == MiniCloud::Client::RequestSendStatus::Accepted)
    {
        m_activationRequestId = result.requestId;
        setAccessState(MiniCloud::Client::ClientAccessState::Authenticating);
    }
    else
    {
        m_activationRequestId = 0;
    }

    return result;
}

void ApplicationController::onResponseReceived(MiniCloud::Client::RequestDestination destination, const MiniCloud::Protocol::ProtocolFrame &frame)
{
    if (destination != MiniCloud::Client::RequestDestination::License
        || frame.header.requestId != m_activationRequestId
        || frame.header.messageType != MiniCloud::Protocol::MessageType::AuthenticateResponse)
    {
        return;
    }

    const MiniCloud::Protocol::AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(frame.payload);
    if (decoded.status != MiniCloud::Protocol::AuthenticateResponseDecodeResult::Status::Success)
    {
        m_activationRequestId = 0;
        setAccessState(MiniCloud::Client::ClientAccessState::Locked);
        emit activationFailed(MiniCloud::Client::RequestDispatchError::InvalidResponsePayload);
        return;
    }

    if (decoded.data.status == MiniCloud::Protocol::AuthenticationStatus::Valid)
    {
        m_activationRequestId = 0;
        setAccessState(MiniCloud::Client::ClientAccessState::Active);
        return;
    }

    m_activationRequestId = 0;
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit activationRejected(decoded.data.status);
}

void ApplicationController::onErrorResponseReceived(
    MiniCloud::Client::RequestDestination destination,
    MiniCloud::Protocol::RequestId requestId,
    MiniCloud::Protocol::TaskId taskId,
    const MiniCloud::Protocol::ErrorResponseData &error)
{
    Q_UNUSED(taskId);

    if (destination != MiniCloud::Client::RequestDestination::License
        || requestId != m_activationRequestId)
    {
        return;
    }

    m_activationRequestId = 0;
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit activationError(error);
}

void ApplicationController::onRequestFailed(
    MiniCloud::Client::RequestDestination destination,
    MiniCloud::Protocol::RequestId requestId,
    MiniCloud::Protocol::TaskId taskId,
    MiniCloud::Client::RequestDispatchError error)
{
    Q_UNUSED(taskId);

    if (destination != MiniCloud::Client::RequestDestination::License
        || requestId != m_activationRequestId)
    {
        return;
    }

    m_activationRequestId = 0;
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit activationFailed(error);
}

void ApplicationController::onDisconnected()
{
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit connectionStateChanged(false);
}

void ApplicationController::setAccessState(MiniCloud::Client::ClientAccessState state)
{
    if (m_accessState == state)
        return;
    m_accessState = state;
    emit accessStateChanged(state);
}
