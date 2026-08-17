#include "protocolcodec.h"
#include <QIODevice>
#include <QIODevice>
namespace MiniCloud::Protocol
{

    QByteArray serializeHeader(const ProtocolHeader &header)
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.setVersion(QDataStream::Qt_6_0);

        stream << header.magic;
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
}