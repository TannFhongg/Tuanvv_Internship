#pragma once
#include <QObject>
#include <QTcpSocket>
#include "frameparser.h"
#include <QByteArray>
#include "protocoltypes.h"
#include "protocolframe.h"
#include <QString>

class ClientSession : public QObject
{
    Q_OBJECT

public:
    explicit ClientSession(QTcpSocket *socket, QObject *parent = nullptr);
    void closeSession();
    bool sendFrame(
        MiniCloud::Protocol::MessageType messageType,
        MiniCloud::Protocol::RequestId requestId,
        MiniCloud::Protocol::TaskId taskId,
        const QByteArray &payload);

signals:
    void sessionFinished(ClientSession *session);
    void frameReceived(ClientSession *session, const MiniCloud::Protocol::ProtocolFrame &frame);
    void protocolError(ClientSession *session, MiniCloud::Protocol::ErrorCode errorCode, const QString &message);

private slots:
    void onDisconnected();
 
private:
    QTcpSocket *m_socket = nullptr;
    MiniCloud::Protocol::FrameParser m_frameParser;
};
