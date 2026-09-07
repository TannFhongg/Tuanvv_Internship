#pragma once

#include <QObject>
#include <QHostAddress>
#include <QString>

#include "ServerRequestDispatcher.h"
#include "TcpServer.h"
#include "filemanager.h"
#include "licensemanager.h"

class ServerController : public QObject
{
    Q_OBJECT

public:
    explicit ServerController(
        QString repositoryFilePath,
        QString storageRoot,
        QObject *parent = nullptr);

    MiniCloud::Server::LicenseManagerResult initialize();

    bool startListening(const QHostAddress &address, quint16 port);

    void stop();

    quint16 serverPort() const;

private:
    MiniCloud::Server::LicenseManager m_licenseManager;
    MiniCloud::Server::FileManager m_fileManager;

    ServerRequestDispatcher m_dispatcher;
    
    TcpServer m_tcpServer;
};
