#include "frameparser.h"
#include "protocolconstants.h"

namespace MiniCloud::Protocol
{
    void FrameParser::appendData(const QByteArray& data)
    {   
        if(data.isEmpty())
        {
            return;
        }
        receiveBuffer.append(data);
    }

    FrameParser::FrameParseResult FrameParser::tryTakeFrame()
    {
        const HeaderDecodeResult decodeResult = deserializeHeader(receiveBuffer);

        if (decodeResult.status == HeaderDecodeStatus::NeedMoreData)
        {
            return {FrameParseStatus::NeedMoreData, {}, ErrorCode::None};
        }

        if (decodeResult.status == HeaderDecodeStatus::Failed)
        {
            return {FrameParseStatus::Failed, {}, decodeResult.errorCode};
        }

        const qsizetype headerSize = static_cast<qsizetype>(protocolWireHeaderSize);
        const qsizetype payloadSize = static_cast<qsizetype>(decodeResult.header.payloadLength);
        const qsizetype frameSize = headerSize + payloadSize;

        if (receiveBuffer.size() < frameSize)
        {
            return {FrameParseStatus::NeedMoreData, {}, ErrorCode::None};
        }

        ProtocolFrame frame;
        frame.header = decodeResult.header;
        frame.payload = receiveBuffer.mid(headerSize, payloadSize);

        receiveBuffer.remove(0, frameSize);

        return {FrameParseStatus::FrameReady, frame, ErrorCode::None};
    }

    void FrameParser::clear()
    {
        receiveBuffer.clear();
    }
}
