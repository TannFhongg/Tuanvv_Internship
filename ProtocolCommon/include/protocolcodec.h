
#pragma once
#include "protocolheader.h"
#include <QByteArray>

namespace MiniCloud::Protocol
{
    QByteArray serializeHeader(const ProtocolHeader &header);
    enum class HeaderDecodeStatus : quint16
    {
        Success,
        NeedMoreData,
        Failed
    };
    struct HeaderDecodeResult
    {
        HeaderDecodeStatus status = HeaderDecodeStatus::Failed;
        ProtocolHeader header{};
        ErrorCode errorCode = ErrorCode::None;
    };
    HeaderDecodeResult deserializeHeader(const QByteArray &data);
}