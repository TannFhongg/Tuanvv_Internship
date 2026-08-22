#include <NetworkClient.h>

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkClient::onErrorOccurred);
    connect(m_socket, &QTcpSocket::stateChanged, this, &NetworkClient::onStateChanged);
}

bool NetworkClient::connectToServer(const QString &hostname, quint16 port)
{
    if (hostname.isEmpty())
    {
        return false;
    }
    if (port == 0)
    {
        return false;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState 
    || m_socket->state() == QAbstractSocket::ConnectingState 
    || m_socket->state() == QAbstractSocket::HostLookupState 
    || m_socket->state() == QAbstractSocket::ClosingState)
    {
        return false;
    }

    m_socket->connectToHost(hostname, port);
    return true;
}

void NetworkClient::disconnectFromServer()
{

    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->disconnectFromHost();
    }

    else if (m_socket->state() == QAbstractSocket::HostLookupState ||
             m_socket->state() == QAbstractSocket::ConnectingState)
    {
        m_socket->abort();
    }
    else if (m_socket->state() == QAbstractSocket::ClosingState || m_socket->state() == QAbstractSocket::UnconnectedState)
    {
    }
}

bool NetworkClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}
QAbstractSocket::SocketState NetworkClient::state() const
{
    return m_socket->state();
}

QString NetworkClient::errorString() const
{
    return m_socket->errorString();
}

void NetworkClient::onConnected()
{
    emit connected();
}

void NetworkClient::onDisconnected()
{
    emit disconnected();
}

void NetworkClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    emit connectionError(socketError, m_socket->errorString());
}

void NetworkClient::onStateChanged(QAbstractSocket::SocketState socketState)
{
    emit stateChanged(socketState);
}
