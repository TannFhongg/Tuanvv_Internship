// ID và các enum protocol.
/*

*/
#pragma once
#include <QtGlobal> // để dùng quint64
namespace MiniCloud::Protocol
{
    using RequestId = quint64;
    using TaskId = quint64;

     enum class MessageType : quint16 {
        Invalid = 0,
        AuthenticateRequest = 1,
        AuthenticateResponse = 2,
        ErrorResponse = 3,
        FileChunk = 4
    };

     enum class ErrorCode : quint16 {
        None = 0,
        InvalidRequest = 1,
        AuthenticationFailed = 2,
        FileNotFound = 3,
        InternalServerError = 4, 
        InvalidFrame = 5,
        UnsupportedProtocolVersion = 6,
        InvalidMessageType = 7,
        PayloadTooLarge = 8,
    };
}
