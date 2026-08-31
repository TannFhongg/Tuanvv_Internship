#include <NetworkClient.h>
#include "protocolcodec.h"
#include "protocolconstants.h"
NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkClient::onErrorOccurred);
    connect(m_socket, &QTcpSocket::stateChanged, this, &NetworkClient::onStateChanged);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkClient::onReadyRead);
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
    if (m_socket->state() == QAbstractSocket::ConnectedState || m_socket->state() == QAbstractSocket::ConnectingState || m_socket->state() == QAbstractSocket::HostLookupState || m_socket->state() == QAbstractSocket::ClosingState)
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
    m_frameParser.clear();
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

bool NetworkClient::sendFrame(MiniCloud::Protocol::MessageType messageType,
                              MiniCloud::Protocol::RequestId requestId,
                              MiniCloud::Protocol::TaskId taskId,
                              const QByteArray &payload)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
    {
        return false;
    }

    const MiniCloud::Protocol::FrameEncodeResult encodeResult = MiniCloud::Protocol::serializeFrame(messageType, requestId, taskId, payload);

    if (encodeResult.status != MiniCloud::Protocol::FrameEncodeStatus::Success)
    {
        return false;
    }

    const qint64 expectedBytes = static_cast<qint64>(encodeResult.encodedFrame.size());
    const qint64 queuedBytes = m_socket->write(encodeResult.encodedFrame);

    if (queuedBytes != expectedBytes)
    {
        m_socket->abort();
        return false;
    }

    return true;
}

void NetworkClient::processBufferedFrames()
{
    while (true)
    {
        const MiniCloud::Protocol::FrameParser::FrameParseResult result = m_frameParser.tryTakeFrame();

        switch (result.status)
        {
        case MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady:
            emit frameReceived(result.frame);
            break;
        case MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData:
            return;
        case MiniCloud::Protocol::FrameParser::FrameParseStatus::Failed:
            emit protocolError(result.errorCode, QStringLiteral("Failed"));
            m_frameParser.clear();
            m_socket->abort();
            return;
        }
    }
}

void NetworkClient::onReadyRead()
{
    const QByteArray data = m_socket->readAll();
    if (data.isEmpty())
    {
        return;
    }
    m_frameParser.appendData(data);

    processBufferedFrames();
}
