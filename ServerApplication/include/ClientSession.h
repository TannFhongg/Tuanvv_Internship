#pragma once
#include <QObject>
#include <QTcpSocket>

class ClientSession : public QObject
{
    Q_OBJECT

public:
    explicit ClientSession(QTcpSocket *socket, QObject *parent = nullptr);
     void closeSession(); 
signals:
    void sessionFinished(ClientSession *session);
    
private slots:
    void onDisconnected();
   
private:
    QTcpSocket *m_socket = nullptr;
};
