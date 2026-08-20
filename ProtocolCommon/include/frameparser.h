#pragma once
#include "protocolheader.h"
#include "protocolcodec.h"
#include "protocolconstants.h"
#include <QByteArray>

namespace MiniCloud::Protocol
{
    class FrameParser
    {
    public:
        struct ProtocolFrame
        {
            ProtocolHeader header;
            QByteArray payload;
        };
        enum class FrameParseStatus
        {
            FrameReady,
            NeedMoreData,
            Failed
        };
        struct FrameParseResult
        {
            FrameParseStatus status = FrameParseStatus::Failed;
            ProtocolFrame frame{};
            ErrorCode errorCode = ErrorCode::None;
        };

        void appendData(const QByteArray &data);
        FrameParseResult tryTakeFrame();
        void clear();
        qsizetype bufferedSize();

    private:
        QByteArray receiveBuffer;
    };

}