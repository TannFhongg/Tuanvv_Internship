#include "servercontroller.h"

#include <utility>

ServerController::ServerController(QString repositoryFilePath, QString storageRoot, QObject *parent)
    : QObject(parent),
      m_licenseManager(std::move(repositoryFilePath)),
      m_fileManager(std::move(storageRoot)),
      m_dispatcher(m_licenseManager, m_fileManager)
{
    connect(
        &m_tcpServer,
        &TcpServer::frameReceived,
        this,
        [this](
            ClientSession *session, const MiniCloud::Protocol::ProtocolFrame &frame)
        {
            if (session != nullptr)
            {
                m_dispatcher.handleFrame(*session, frame);
            }
        });

    connect(
        &m_tcpServer,
        &TcpServer::clientDisconnected,
        this,
        [this]()
        {
            m_dispatcher.cancelActiveUpload();
        });
}

MiniCloud::Server::LicenseManagerResult ServerController::initialize()
{
    return m_licenseManager.initialize();
}

bool ServerController::startListening(const QHostAddress &address, quint16 port)
{
    return m_tcpServer.startListening(address, port);
}

void ServerController::stop()
{
    m_tcpServer.stop();
}

bool ServerController::isListening() const
{
    return m_tcpServer.isListening();
}

quint16 ServerController::serverPort() const
{
    return m_tcpServer.serverPort();
}

QString ServerController::lastError() const
{
    return m_tcpServer.errorString();
}

MiniCloud::Server::LicenseManager &ServerController::licenseManager() noexcept
{
    return m_licenseManager;
}
