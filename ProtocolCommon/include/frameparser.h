#pragma once
#include "protocolframe.h"
#include <QByteArray>

namespace MiniCloud::Protocol
{
    class FrameParser
    {
    public:
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
        qsizetype bufferedSize() const;

    private:
        QByteArray receiveBuffer;
    };
}