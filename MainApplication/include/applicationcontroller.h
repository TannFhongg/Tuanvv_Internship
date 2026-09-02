#pragma once

#include <QMetaType>
#include <QObject>
#include "NetworkClient.h"
#include "requestdispatcher.h"
#include "requesttypes.h"
#include "authentication.h"
#include "errorresponse.h"

namespace MiniCloud::Client
{
    enum class ClientAccessState
    {
        Locked,
        Authenticating,
        Active
    };
}

class ApplicationController : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationController(QObject *parent = nullptr);
    ApplicationController(int requestTimeoutMs, QObject *parent = nullptr);

    MiniCloud::Client::ClientAccessState accessState() const noexcept;
    bool isFeatureAccessAllowed() const noexcept;

    bool connectToServer(const QString &hostname, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    MiniCloud::Client::RequestSendResult activate(const QString &licenseKey, const QString &deviceId);

signals:
    void accessStateChanged(MiniCloud::Client::ClientAccessState state);
    void activationRejected(MiniCloud::Protocol::AuthenticationStatus status);
    void activationError(const MiniCloud::Protocol::ErrorResponseData &error);
    void activationFailed(MiniCloud::Client::RequestDispatchError error);
    void connectionStateChanged(bool connected);
    void connectionFailed(const QString &message);

private slots:
    void onResponseReceived(MiniCloud::Client::RequestDestination destination, const MiniCloud::Protocol::ProtocolFrame &frame);

    void onErrorResponseReceived(
        MiniCloud::Client::RequestDestination destination,
        MiniCloud::Protocol::RequestId requestId,
        MiniCloud::Protocol::TaskId taskId,
        const MiniCloud::Protocol::ErrorResponseData &error);

    void onRequestFailed(
        MiniCloud::Client::RequestDestination destination,
        MiniCloud::Protocol::RequestId requestId,
        MiniCloud::Protocol::TaskId taskId,
        MiniCloud::Client::RequestDispatchError error);
        
    void onDisconnected();

private:
    MiniCloud::Client::ClientAccessState m_accessState{MiniCloud::Client::ClientAccessState::Locked};

    NetworkClient m_networkClient;
    RequestDispatcher m_requestDispatcher;
    MiniCloud::Protocol::RequestId m_activationRequestId{0};

    void setAccessState(MiniCloud::Client::ClientAccessState state);
};

Q_DECLARE_METATYPE(MiniCloud::Client::ClientAccessState)
