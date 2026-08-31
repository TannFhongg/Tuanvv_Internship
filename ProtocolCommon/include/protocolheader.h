
#pragma once
#include <QtGlobal> 
#include "protocoltypes.h"

namespace MiniCloud::Protocol {
    
    struct ProtocolHeader
    {
        quint32 protocolMagic = 0;
        quint16 protocolVersion = 0;
        MessageType messageType = MessageType::Invalid;
        quint32 payloadLength = 0;
        RequestId requestId = 0;
        TaskId taskId = 0;
    };
} 
