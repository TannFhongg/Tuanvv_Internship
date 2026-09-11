#pragma once

#include <QHostAddress>
#include <QString>
#include <QtGlobal>

#include <memory>

class ServerController;

namespace MiniCloud::Server
{
    class LicenseManager;
}

class ServerApplicationRuntime
{
public:
    struct Configuration
    {
        QHostAddress address{QHostAddress::AnyIPv4};
        quint16 port{0};
        QString licenseStoragePath;
        QString fileStorageRoot;
    };

    explicit ServerApplicationRuntime(Configuration configuration);
    ~ServerApplicationRuntime();

    ServerApplicationRuntime(const ServerApplicationRuntime &) = delete;
    ServerApplicationRuntime &operator=(const ServerApplicationRuntime &) = delete;

    bool start();
    void stop();

    bool isListening() const;
    quint16 boundPort() const;
    QString lastError() const;

    MiniCloud::Server::LicenseManager *licenseManager() noexcept;

private:
    Configuration m_configuration;
    std::unique_ptr<ServerController> m_serverController;
    QString m_lastError;
};
