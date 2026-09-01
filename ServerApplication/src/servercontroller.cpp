#include "servercontroller.h"

#include <utility>

ServerController::ServerController(QString repositoryFilePath, QObject *parent)
    : QObject(parent), m_licenseManager(std::move(repositoryFilePath)), m_dispatcher(m_licenseManager)
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

quint16 ServerController::serverPort() const
{
    return m_tcpServer.serverPort();
}
