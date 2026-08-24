#include "ClientSession.h"
#include "protocolcodec.h"
#include "frameparser.h"
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
    connect(m_socket,
            &QTcpSocket::readyRead,
            this,
            &ClientSession::onReadyRead);
}

void ClientSession::onDisconnected()
{
    m_frameParser.clear();
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

void ClientSession::onReadyRead()
{
    const QByteArray data = m_socket->readAll();

    if (data.isEmpty())
    {
        return;
    }

    m_frameParser.appendData(data);
    processBufferedFrames();
}

void ClientSession::processBufferedFrames()
{
    for (;;)
    {
        const MiniCloud::Protocol::FrameParser::FrameParseResult result = m_frameParser.tryTakeFrame();

        if (result.status == MiniCloud::Protocol::FrameParser::FrameParseStatus::Failed)
        {
            emit protocolError(this, result.errorCode, QStringLiteral("Failed"));
            m_frameParser.clear();
            m_socket->abort();
            return;
        }

        if (result.status == MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData)
        {
            return;
        }

        if (result.status == MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady)
        {
            emit frameReceived(this, result.frame);
            break;
        }
    }
}