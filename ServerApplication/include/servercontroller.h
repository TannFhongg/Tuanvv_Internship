#pragma once

#include <QObject>
#include <QHostAddress>
#include <QString>

#include "ServerRequestDispatcher.h"
#include "TcpServer.h"
#include "licensemanager.h"

class ServerController : public QObject
{
    Q_OBJECT

public:
    explicit ServerController(
        QString repositoryFilePath,
        QObject *parent = nullptr);

    MiniCloud::Server::LicenseManagerResult initialize();

    bool startListening(const QHostAddress &address, quint16 port);

    void stop();

    quint16 serverPort() const;

private:
    MiniCloud::Server::LicenseManager m_licenseManager;

    ServerRequestDispatcher m_dispatcher;
    
    TcpServer m_tcpServer;
};
