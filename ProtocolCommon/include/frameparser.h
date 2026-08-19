#include "protocolheader.h"
#include "protocolcodec.h"
#include "protocolconstants.h"

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

        ProtocolFrame appendData();
        FrameParseResult tryTakeFrame();
        void clear();

    private:
        QByteArray receiveBuffer;
    };

}