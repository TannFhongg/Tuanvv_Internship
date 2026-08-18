#include "protocolcodec.h"
#include "protocolconstants.h"
#include <QtTest/QTest>

class ProtocolCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void encode()
    {
        MiniCloud::Protocol::ProtocolHeader header;
        header.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header.payloadLength = 0;
        header.requestId = 0;
        header.taskId = 0;

        const QByteArray encoded = MiniCloud::Protocol::serializeHeader(header);
        QCOMPARE(encoded.size(), static_cast<qsizetype>(MiniCloud::Protocol::protocolWireHeaderSize));
        qDebug() << "Encoded header size:" << encoded.size();
    }

    void testBigEndian()
    {
        MiniCloud::Protocol::ProtocolHeader header;
        header.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header.payloadLength = 5;
        header.requestId = 0x0102030405060708 ;
        header.taskId = 0;
        const QByteArray encoded = MiniCloud::Protocol::serializeHeader(header);
        const QByteArray expected = QByteArray::fromHex("4D434C44000100010000000501020304050607080000000000000000");
        QCOMPARE(encoded,expected);

    }

};
QTEST_APPLESS_MAIN(ProtocolCodecTest)
#include "protocolcodectest.moc"
