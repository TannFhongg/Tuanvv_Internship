#pragma once
#include <QTcpServer>
#include <QObject>
#include <QHostAddress>
#include "protocolframe.h"

class ClientSession;

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    bool startListening(const QHostAddress &address, quint16 port);
    void stop();
    bool isListening() const;
    QString errorString() const;
    quint16 serverPort() const;

signals:
    void clientConnected();
    void clientDisconnected();
    void clientRejected();
    void listenFailed(const QString &errorString);
    void frameReceived(
        ClientSession *session,
        const MiniCloud::Protocol::ProtocolFrame &frame);

private slots:
    void onNewConnection();
    void onSessionFinished(ClientSession *session);

private:
    QTcpServer *m_server;
    ClientSession *m_activeSession = nullptr;
};
