#pragma once

#include "protocolframe.h"

class ClientSession;

namespace MiniCloud::Server
{
    class LicenseManager;
}

class ServerRequestDispatcher
{
public:
    explicit ServerRequestDispatcher(MiniCloud::Server::LicenseManager &licenseManager);

    void handleFrame(ClientSession &session, const MiniCloud::Protocol::ProtocolFrame &frame);

private:
    MiniCloud::Server::LicenseManager &m_licenseManager;
};
