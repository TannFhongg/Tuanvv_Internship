#include <QtTest/QTest>
#include "frameparser.h"
#include "protocolheader.h"
class FrameParserTest : public QObject
{
    Q_OBJECT

public:
    QByteArray makeFrame(const MiniCloud::Protocol::ProtocolHeader &header, const QByteArray &payload)
    {
        MiniCloud::Protocol::ProtocolHeader protocolHeader = header;
        protocolHeader.payloadLength = static_cast<quint32>(payload.size());

        const QByteArray encodedHeader = MiniCloud::Protocol::serializeHeader(protocolHeader);
        return encodedHeader + payload;
    }
private slots:

    void tryTakeFrame_emptyBuffer_returnsNeedMoreData()
    {
        MiniCloud::Protocol::FrameParser parser;
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(0));
        const MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
        parser.bufferedSize();
    }

    void tryTakeFrame_partialHeader_returnsNeedMoreData()
    {
        MiniCloud::Protocol::FrameParser parser;
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "0001");
        parser.appendData(input);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(input.size()));
        const MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void tryTakeFrame_partialPayload_returnsNeedMoreData()
    {
        MiniCloud::Protocol::FrameParser parser;
        MiniCloud::Protocol::ProtocolHeader header;
        header.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header.requestId =0x0102030405060708ULL;
        header.taskId = 0;
        const QByteArray payload = QByteArray::fromHex("DE AD BB CC");

        QByteArray fullFrame = makeFrame(header, payload);
        QByteArray partialFrame = fullFrame.left(fullFrame.size() - 1);
        parser.appendData(partialFrame);
        QCOMPARE(parser.bufferedSize(), partialFrame.size());

        const MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void tryTakeFrame_completeFrame_returnsFrameReady() {
        MiniCloud::Protocol::FrameParser parser;
        const QByteArray input = QByteArray::fromHex(
            "4D434C44"
            "0001"
            "0001"
            "00000005"
            "0102030405060708"
            "0000000000000000");
        const QByteArray payload(static_cast<qsizetype>(5), '\0');
        qDebug() << payload.size();
        parser.appendData(input + payload);
        MiniCloud::Protocol::deserializeHeader(input);
        const qsizetype expectedSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolWireHeaderSize + payload.size());
        QCOMPARE(parser.bufferedSize(), expectedSize);
        const MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }
};

QTEST_APPLESS_MAIN(FrameParserTest)
#include "frameparsertest.moc"