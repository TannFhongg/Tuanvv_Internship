#include "ClientSession.h"
#include <QtGlobal>

ClientSession::ClientSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket)
{
    Q_ASSERT(m_socket);
    m_socket->setParent(this);

    connect(m_socket,
            &QTcpSocket::disconnected,
            this,
            &ClientSession::onDisconnected);
}

void ClientSession::onDisconnected()
{
    emit sessionFinished(this);
}

void ClientSession::closeSession()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_socket->disconnectFromHost();
    }
}
