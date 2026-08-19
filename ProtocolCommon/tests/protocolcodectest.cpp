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
        header.requestId = 0x0102030405060708;
        header.taskId = 0;
        const QByteArray encoded = MiniCloud::Protocol::serializeHeader(header);
        const QByteArray expected = QByteArray::fromHex("4D434C44000100010000000501020304050607080000000000000000");
        QCOMPARE(encoded, expected);
    }

    void deserialize_validHeader_returnsSuccess()
    {
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "0001"
            "00000005"
            "0102030405060708"
            "0000000000000000");

        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);

        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::Success);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
        QCOMPARE(result.header.protocolMagic, MiniCloud::Protocol::protocolMagic);
        QCOMPARE(result.header.protocolVersion, MiniCloud::Protocol::protocolVersion);
        QCOMPARE(result.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(result.header.payloadLength, static_cast<quint32>(5));
        QCOMPARE(result.header.requestId, static_cast<quint64>(0x0102030405060708));
        QCOMPARE(result.header.taskId, static_cast<quint64>(0));
    }

    void deserialize_lessThanFourBytes_returnsNeedMoreData()
    {
        const QByteArray input = QByteArray::fromHex("4D434C");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);
        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::NeedMoreData);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void deserialize_validMagicButIncomplete_returnsNeedMoreData()
    {
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "0001"
            "00000005"
            "0102030405060708");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);
        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::NeedMoreData);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void deserialize_invalidMagic_returnsInvalidFrame()
    {
        const QByteArray input = QByteArray::fromHex(
            "00434C44"
            "0001"
            "0001"
            "00000005"
            "0102030405060708"
            "0000000000000000");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);
        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::Failed);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::InvalidFrame);
    }

    void deserialize_unsupportedVersion_returnsError()
    {
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0002"
            "0001"
            "00000005"
            "0102030405060708"
            "0000000000000000");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);
        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::Failed);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::UnsupportedProtocolVersion);
    }

    void deserialize_unknownMessageType_returnsError()
    {
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "03E7"
            "00000005"
            "0102030405060708"
            "0000000000000000");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);

        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::Failed);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::InvalidMessageType);
    }

    void deserialize_payloadTooLarge_returnsError()
    {
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "0001"
            "00100001"
            "0102030405060708"
            "0000000000000000");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);

        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::Failed);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::PayloadTooLarge);
    }

    void deserialize_headerWithFollowingPayload_returnsSuccess()
    {

        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "0001"
            "00000005"
            "0102030405060708"
            "0000000000000000"
            "DEADBEEF01");
        const MiniCloud::Protocol::HeaderDecodeResult result = MiniCloud::Protocol::deserializeHeader(input);
        QCOMPARE(result.status, MiniCloud::Protocol::HeaderDecodeStatus::Success);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
        QCOMPARE(result.header.protocolMagic, MiniCloud::Protocol::protocolMagic);
        QCOMPARE(result.header.protocolVersion, MiniCloud::Protocol::protocolVersion);
        QCOMPARE(result.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(result.header.payloadLength, static_cast<quint32>(5));
        QCOMPARE(result.header.requestId, static_cast<quint64>(0x0102030405060708));
        QCOMPARE(result.header.taskId, static_cast<quint64>(0));
    }
};

QTEST_APPLESS_MAIN(ProtocolCodecTest)
#include "protocolcodectest.moc"
