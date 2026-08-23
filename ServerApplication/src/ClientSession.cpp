#include "ClientSession.h"
#include "protocolcodec.h"
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
 
bool ClientSession::sendFrame(
    MiniCloud::Protocol::MessageType messageType,
    MiniCloud::Protocol::RequestId requestId,
    MiniCloud::Protocol::TaskId taskId,
    const QByteArray &payload)
{
    if (m_socket == nullptr || m_socket->state() != QAbstractSocket::ConnectedState)
        return false;

    const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);

    if (encodeResult.status != MiniCloud::Protocol::FrameEncodeStatus::Success)
    {
        return false;
    }

    const qint64 expectedBytes = static_cast<qint64>(encodeResult.encodedFrame.size());
    const qint64 queuedBytes = m_socket->write(encodeResult.encodedFrame);
    if (queuedBytes != expectedBytes)
    {
        return false;
    }
    return true;
}