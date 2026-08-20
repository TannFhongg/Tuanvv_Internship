
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

    enum class FrameEncodeStatus { 
        Success,
        Failed
    };  

    struct FrameEncodeResult { 
        FrameEncodeStatus status = FrameEncodeStatus::Failed;
        QByteArray encodedFrame; 
        ErrorCode errorCode = ErrorCode::InvalidFrame;
    }; 

    FrameEncodeResult serializeFrame(MessageType messageType, RequestId requestId, TaskId taskId, const QByteArray &payload);
                

    
    HeaderDecodeResult deserializeHeader(const QByteArray &data);
}