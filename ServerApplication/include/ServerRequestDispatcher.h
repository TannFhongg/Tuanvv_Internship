#pragma once

#include <optional>

#include "protocolframe.h"

class ClientSession;

namespace MiniCloud::Server
{
    class FileManager;
    class LicenseManager;
}

class ServerRequestDispatcher
{
public:
    explicit ServerRequestDispatcher(MiniCloud::Server::LicenseManager &licenseManager);

    ServerRequestDispatcher(MiniCloud::Server::LicenseManager &licenseManager, MiniCloud::Server::FileManager &fileManager);

    void handleFrame(ClientSession &session, const MiniCloud::Protocol::ProtocolFrame &frame);

    void cancelActiveUpload();

private:
    MiniCloud::Server::LicenseManager &m_licenseManager;
    MiniCloud::Server::FileManager *m_fileManager = nullptr;
    std::optional<MiniCloud::Protocol::RequestId> m_activeUploadRequestId;
};
