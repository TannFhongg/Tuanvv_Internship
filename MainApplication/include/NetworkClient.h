#pragma once
#include <QTcpSocket>
#include <QObject>
#include <QString>

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

signals:
    void connected();
    void disconnected();
    void connectionError(QAbstractSocket::SocketError socketError, const QString &message);
    void stateChanged(QAbstractSocket::SocketState socketState);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onStateChanged(QAbstractSocket::SocketState socketState);

private:
    QTcpSocket *m_socket;
};