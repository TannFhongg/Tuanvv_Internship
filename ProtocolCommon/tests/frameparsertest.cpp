#include <QtTest/QTest>
#include "frameparser.h"

class FrameParserTest : public QObject
{
    Q_OBJECT

public:
    QByteArray makeFrame(const ProtocolHeader &header, const QByteArray &payload)
    {
        MiniCloud::Protocol::ProtocolHeader protocolHeader = header;
        protocolHeader.payloadLength = static_cast<quint32>(payload.size());

        const QByteArray encodedHeader = MiniCloud::Protocol::serializeHeader(protocolHeader);
        return encodedHeader + payload;
    }

};