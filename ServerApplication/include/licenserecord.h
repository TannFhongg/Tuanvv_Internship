#pragma once 
#include <QString>

namespace MiniCloud::Server {
    struct LicenseRecord
    {
        QString productKey;
        QString deviceId;
        bool enabled {true};
    };
};