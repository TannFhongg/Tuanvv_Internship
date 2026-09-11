#include "serverapplicationruntime.h"

#include "servercontroller.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

ServerApplicationRuntime::ServerApplicationRuntime(Configuration configuration)
    : m_configuration(std::move(configuration))
{
}

ServerApplicationRuntime::~ServerApplicationRuntime()
{
    stop();
}

bool ServerApplicationRuntime::start()
{
    if (isListening())
    {
        m_lastError = QStringLiteral("Server is already listening.");
        return false;
    }

    if (m_configuration.licenseStoragePath.isEmpty())
    {
        m_lastError = QStringLiteral("License storage path is required.");
        return false;
    }

    if (m_configuration.fileStorageRoot.isEmpty())
    {
        m_lastError = QStringLiteral("File storage root is required.");
        return false;
    }

    const QFileInfo licenseStorageInfo(m_configuration.licenseStoragePath);
    if (!QDir().mkpath(licenseStorageInfo.absolutePath()))
    {
        m_lastError = QStringLiteral("Unable to create the license storage directory.");
        return false;
    }

    if (!QDir().mkpath(m_configuration.fileStorageRoot))
    {
        m_lastError = QStringLiteral("Unable to create the file storage directory.");
        return false;
    }

    auto serverController = std::make_unique<ServerController>(
        m_configuration.licenseStoragePath,
        m_configuration.fileStorageRoot);
    const MiniCloud::Server::LicenseManagerResult initializeResult = serverController->initialize();
    if (initializeResult.status != MiniCloud::Server::LicenseManagerOperationStatus::Success)
    {
        m_lastError = initializeResult.errorMessage;
        return false;
    }

    if (!serverController->startListening(m_configuration.address, m_configuration.port))
    {
        m_lastError = serverController->lastError();
        return false;
    }

    m_serverController = std::move(serverController);
    m_lastError.clear();
    return true;
}

void ServerApplicationRuntime::stop()
{
    if (m_serverController)
    {
        m_serverController->stop();
    }
}

bool ServerApplicationRuntime::isListening() const
{
    return m_serverController && m_serverController->isListening();
}

quint16 ServerApplicationRuntime::boundPort() const
{
    return m_serverController ? m_serverController->serverPort() : 0;
}

QString ServerApplicationRuntime::lastError() const
{
    return m_lastError;
}

MiniCloud::Server::LicenseManager *ServerApplicationRuntime::licenseManager() noexcept
{
    return m_serverController ? &m_serverController->licenseManager() : nullptr;
}
