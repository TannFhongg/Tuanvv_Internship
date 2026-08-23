#pragma once
#include <QTcpSocket>
#include <QObject>
#include <QByteArray>
#include <QString>
#include "protocoltypes.h"
#include "protocolframe.h"
#include "frameparser.h"


class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);
    bool connectToServer(const QString &hostname, quint16 port);
    void disconnectFromServer();

    bool isConnected() const;
    QAbstractSocket::SocketState state() const;
    QString errorString() const;

    bool sendFrame(MiniCloud::Protocol::MessageType messageType,
                   MiniCloud::Protocol::RequestId requestId,
                   MiniCloud::Protocol::TaskId taskId,
                   const QByteArray &payload);
    
    
    
signals:
    void connected();
    void disconnected();
    void connectionError(QAbstractSocket::SocketError socketError, const QString &message);
    void stateChanged(QAbstractSocket::SocketState socketState);
    
    void frameReceived(const MiniCloud::Protocol::ProtocolFrame& frame); 
    void protocolError(MiniCloud::Protocol::ErrorCode errorCode, const QString& message);     

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onStateChanged(QAbstractSocket::SocketState socketState);

    void onReadyRead();

private:
    QTcpSocket *m_socket;
    MiniCloud::Protocol::FrameParser m_frameParser;
    void processBufferedFrames(); 
};
 