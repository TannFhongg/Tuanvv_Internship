#pragma once

#include "protocoltypes.h"
#include "protocolheader.h"
#include <QByteArray>
#include <QMetaType>

namespace MiniCloud::Protocol
{
    struct ProtocolFrame
    {
        ProtocolHeader header;
        QByteArray payload;
    };
}

Q_DECLARE_METATYPE(MiniCloud::Protocol::ProtocolFrame)
