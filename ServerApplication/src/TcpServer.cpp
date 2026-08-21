#include "TcpServer.h"
#include "ClientSession.h"
#include <QtGlobal>

TcpServer::TcpServer(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this)), m_activeSession(nullptr)
{
    connect(m_server,                     // doi tuong phat signal
            &QTcpServer::newConnection,   // signal phat ra khi co ket noi moi
            this,                         // doi tuong nhan signal , TcpServer hien tai
            &TcpServer::onNewConnection); // slot duoc goi khi signal duoc phat ra
}

bool TcpServer::startListening(const QHostAddress &address, quint16 port)
{
    if (m_server->isListening())
    {
        emit listenFailed("Server is already listening.");
        return false;
    }
    else if (!m_server->listen(address, port))
    {
        emit listenFailed(m_server->errorString());
        return false;
    }
    return true;
}

void TcpServer::stop()
{
    if (m_server->isListening())
    {
        m_server->close();
    }

    if (m_activeSession)
    {
        m_activeSession->closeSession();
    }
}

bool TcpServer::isListening() const
{
    return m_server->isListening();
}

QString TcpServer::errorString() const
{
    return m_server->errorString();
}

quint16 TcpServer::serverPort() const
{
    return m_server->serverPort();
}

void TcpServer::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();

    if (!socket)
    {
        return;
    }

    if(m_activeSession)
    {
        socket->disconnectFromHost();
        socket->deleteLater();
        emit clientRejected();
        return;
    }

    m_activeSession = new ClientSession(socket, this);
    connect(m_activeSession, &ClientSession::sessionFinished, this, &TcpServer::onSessionFinished);
    emit clientConnected();
}
void TcpServer::onSessionFinished(ClientSession *session)
{
    if (session == nullptr)
        return;

    if (session != m_activeSession)
    {
        session->deleteLater();
        return;
    }
    m_activeSession = nullptr;
    session->deleteLater();
    emit clientDisconnected();
}
