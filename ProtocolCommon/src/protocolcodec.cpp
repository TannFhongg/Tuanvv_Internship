#include "protocolcodec.h"
#include "protocolconstants.h"
#include "protocoltypes.h"
#include <QIODevice>
#include <QDataStream>
namespace MiniCloud::Protocol
{
    namespace
    {
        bool isValidMessageType(quint16 value)
        {
            switch (static_cast<MessageType>(value))
            {
            case MessageType::AuthenticateRequest:
            case MessageType::AuthenticateResponse:
            case MessageType::ErrorResponse:
            case MessageType::FileChunk:
            case MessageType::BrowseRequest:
            case MessageType::BrowseResponse:
            case MessageType::CreateDirectoryRequest:
            case MessageType::FileOperationResponse:
            case MessageType::SearchRequest:
            case MessageType::SearchResponse:
                return true;
            case MessageType::Invalid:
            default:
                return false;
            }
        }
    }

    QByteArray serializeHeader(const ProtocolHeader &header)
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.setVersion(QDataStream::Qt_6_0);

        stream << header.protocolMagic;
        stream << header.protocolVersion;
        stream << static_cast<quint16>(header.messageType);
        stream << header.payloadLength;
        stream << header.requestId;
        stream << header.taskId;

        if (stream.status() != QDataStream::Ok)
        {
            return {};
        }
        return data;
    }

    HeaderDecodeResult MiniCloud::Protocol::deserializeHeader(const QByteArray &data)
    {
        if (data.size() < static_cast<qsizetype>(sizeof(quint32)))

        {
            return {HeaderDecodeStatus::NeedMoreData, ProtocolHeader{}, ErrorCode::None};
        }

        QDataStream stream(data);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.setVersion(QDataStream::Qt_6_0);

        quint32 recvMagic = 0;
        stream >> recvMagic;

        if (stream.status() != QDataStream::Ok)
        {
            return {HeaderDecodeStatus::Failed, ProtocolHeader{}, ErrorCode::InvalidFrame};
        }

        if (recvMagic != protocolMagic)
        {
            return {HeaderDecodeStatus::Failed, ProtocolHeader{}, ErrorCode::InvalidFrame};
        }

        if (data.size() < static_cast<qsizetype>(protocolWireHeaderSize))
        {
            return {HeaderDecodeStatus::NeedMoreData, ProtocolHeader{}, ErrorCode::None};
        }

        quint16 recvVersion = 0;
        stream >> recvVersion;
        quint16 rawMessageType = 0;
        stream >> rawMessageType;
        quint32 recvPayloadLength = 0;
        stream >> recvPayloadLength;
        RequestId recvRequestId = 0;
        stream >> recvRequestId;
        TaskId recvTaskId = 0;
        stream >> recvTaskId;

        if (stream.status() != QDataStream::Ok)
        {
            return {HeaderDecodeStatus::Failed, ProtocolHeader{}, ErrorCode::InvalidFrame};
        }
        if (recvVersion != protocolVersion)
        {
            return {HeaderDecodeStatus::Failed, ProtocolHeader{}, ErrorCode::UnsupportedProtocolVersion};
        }
        if (!isValidMessageType(rawMessageType))
        {
            return {HeaderDecodeStatus::Failed, ProtocolHeader{}, ErrorCode::InvalidMessageType};
        }
        if (recvPayloadLength > protocolMaxFramePayloadSize)
        {
            return {HeaderDecodeStatus::Failed, ProtocolHeader{}, ErrorCode::PayloadTooLarge};
        }
        ProtocolHeader header;
        header.protocolMagic = recvMagic;
        header.protocolVersion = recvVersion;
        header.messageType = static_cast<MessageType>(rawMessageType);
        header.payloadLength = recvPayloadLength;
        header.requestId = recvRequestId;
        header.taskId = recvTaskId;

        return {HeaderDecodeStatus::Success, header, ErrorCode::None};
    }

    FrameEncodeResult serializeFrame(MessageType messageType, RequestId requestId, TaskId taskId, const QByteArray &payload)
    {
        MiniCloud::Protocol::MessageType msgType = messageType;
        if (isValidMessageType(static_cast<quint16>(msgType)) == false)
        {
            return {FrameEncodeStatus::Failed, QByteArray(), MiniCloud::Protocol::ErrorCode::InvalidMessageType};
        }

        if (payload.size() > static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFramePayloadSize))
        {
            return {FrameEncodeStatus::Failed, QByteArray(), MiniCloud::Protocol::ErrorCode::PayloadTooLarge};
        }

        MiniCloud::Protocol::ProtocolHeader header;
        header.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header.messageType = msgType;
        header.payloadLength = static_cast<quint32>(payload.size());
        header.requestId = requestId;
        header.taskId = taskId;

        QByteArray headerData = MiniCloud::Protocol::serializeHeader(header);
        if (headerData.isEmpty())
        {
            return {FrameEncodeStatus::Failed, QByteArray(), MiniCloud::Protocol::ErrorCode::InvalidFrame};
        }

        QByteArray frameData = headerData + payload;
        return {FrameEncodeStatus::Success, frameData, MiniCloud::Protocol::ErrorCode::None};
    }
}
