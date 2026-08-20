#include <QtTest/QTest>
#include "frameparser.h"
#include "protocolheader.h"
#include "protocolconstants.h"

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
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(input.size()));
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
        header.requestId = 0x0102030405060708ULL;
        header.taskId = 0;
        const QByteArray payload = QByteArray::fromHex("DE AE BE EF 01");

        QByteArray fullFrame = makeFrame(header, payload);
        QByteArray partialFrame = fullFrame.left(fullFrame.size() - 3);
        parser.appendData(partialFrame);
        QCOMPARE(parser.bufferedSize(), partialFrame.size());

        const MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void tryTakeFrame_completeFrame_returnsFrameReady()
    {
        MiniCloud::Protocol::FrameParser parser;
        MiniCloud::Protocol::ProtocolHeader header;
        header.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header.requestId = 0;
        header.taskId = 0;

        const QByteArray payload = QByteArray::fromHex("DE AD BE EF 01");
        const QByteArray input = makeFrame(header, payload);
        parser.appendData(input);
        const qsizetype expectedSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolWireHeaderSize + payload.size());
        QCOMPARE(parser.bufferedSize(), expectedSize);
        const MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();

        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result.frame.header.protocolMagic, MiniCloud::Protocol::protocolMagic);
        QCOMPARE(result.frame.header.protocolVersion, MiniCloud::Protocol::protocolVersion);
        QCOMPARE(result.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(result.frame.header.payloadLength, static_cast<quint32>(5));
        QCOMPARE(result.frame.header.requestId, static_cast<quint64>(0));
        QCOMPARE(result.frame.header.taskId, static_cast<quint64>(0));
        QCOMPARE(result.frame.payload, payload);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(0));
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void tryTakeFrame_fragmentedFrame_returnsReadyAfterRemainderAppended()
    {
        MiniCloud::Protocol::FrameParser parser;
        MiniCloud::Protocol::ProtocolHeader header;
        header.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header.requestId = 0;
        header.taskId = 0;

        const QByteArray payload = QByteArray::fromHex("DE AD BE EF 01");
        const QByteArray fullFrame = makeFrame(header, payload);

        parser.appendData(fullFrame.left(fullFrame.size() - 3));
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(fullFrame.size() - 3));
        MiniCloud::Protocol::FrameParser::FrameParseResult result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        parser.appendData(fullFrame.right(3));
        result = parser.tryTakeFrame();
        QCOMPARE(result.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result.frame.payload, payload);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(0));
        QCOMPARE(result.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void tryTakeFrame_twoCompleteFrames_returnsOneAtATime()
    {
        MiniCloud::Protocol::FrameParser parser;
        MiniCloud::Protocol::ProtocolHeader header_A;
        header_A.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header_A.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header_A.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header_A.requestId = 101;
        header_A.taskId = 0;

        const QByteArray payload_A = QByteArray::fromHex("DE AD BE EF 01");
        const QByteArray frame_A = makeFrame(header_A, payload_A);

        MiniCloud::Protocol::ProtocolHeader header_B;
        header_B.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header_B.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header_B.messageType = MiniCloud::Protocol::MessageType::AuthenticateResponse;
        header_B.requestId = 102;
        header_B.taskId = 0;

        const QByteArray payload_B = QByteArray::fromHex("CA FE BA BE 02");
        const QByteArray frame_B = makeFrame(header_B, payload_B);

        parser.appendData(frame_A + frame_B);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(frame_A.size() + frame_B.size()));
        MiniCloud::Protocol::FrameParser::FrameParseResult result_A = parser.tryTakeFrame();
        QCOMPARE(result_A.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_A.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(result_A.frame.payload, payload_A);
        QCOMPARE(result_A.errorCode, MiniCloud::Protocol::ErrorCode::None);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_B = parser.tryTakeFrame();
        QCOMPARE(result_B.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_B.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateResponse);
        QCOMPARE(result_B.frame.payload, payload_B);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(0));
        QCOMPARE(result_B.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }

    void tryTakeFrame_completeFrameAndPartialNext_preservesPartialNextFrame()
    {
        MiniCloud::Protocol::FrameParser parser;
        MiniCloud::Protocol::ProtocolHeader header_A;
        header_A.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header_A.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header_A.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header_A.requestId = 101;
        header_A.taskId = 0;

        const QByteArray payload_A = QByteArray::fromHex("CA FE BA BE 02");
        const QByteArray frame_A = makeFrame(header_A, payload_A);

        MiniCloud::Protocol::ProtocolHeader header_B;
        header_B.protocolMagic = MiniCloud::Protocol::protocolMagic;
        header_B.protocolVersion = MiniCloud::Protocol::protocolVersion;
        header_B.messageType = MiniCloud::Protocol::MessageType::AuthenticateRequest;
        header_B.requestId = 102;
        header_B.taskId = 0;
        const QByteArray payload_B = QByteArray::fromHex("CA FE BA BE 02");
        const QByteArray frame_B = makeFrame(header_B, payload_B);
        const qsizetype partialFrame_B = 10;

        parser.appendData(frame_A + frame_B.left(partialFrame_B));
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(frame_A.size() + partialFrame_B));
        MiniCloud::Protocol::FrameParser::FrameParseResult result_A = parser.tryTakeFrame();
        QCOMPARE(result_A.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_A.frame.header.protocolMagic, MiniCloud::Protocol::protocolMagic);
        QCOMPARE(result_A.frame.header.protocolVersion, MiniCloud::Protocol::protocolVersion);
        QCOMPARE(result_A.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(result_A.frame.header.payloadLength, static_cast<quint32>(5));
        QCOMPARE(result_A.frame.header.requestId, static_cast<quint64>(101));
        QCOMPARE(result_A.frame.header.taskId, static_cast<quint64>(0));
        QCOMPARE(result_A.frame.payload, payload_A);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(partialFrame_B));
        QCOMPARE(result_A.errorCode, MiniCloud::Protocol::ErrorCode::None);

        MiniCloud::Protocol::FrameParser::FrameParseResult result_B = parser.tryTakeFrame();
        QCOMPARE(result_B.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::NeedMoreData);
        QCOMPARE(result_B.errorCode, MiniCloud::Protocol::ErrorCode::None);
        QCOMPARE(parser.bufferedSize(), partialFrame_B);
        parser.appendData(frame_B.right(frame_B.size() - partialFrame_B));
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(frame_B.size()));
        result_B = parser.tryTakeFrame();
        QCOMPARE(result_B.frame.header.protocolMagic, MiniCloud::Protocol::protocolMagic);
        QCOMPARE(result_B.frame.header.protocolVersion, MiniCloud::Protocol::protocolVersion);
        QCOMPARE(result_B.frame.header.messageType, MiniCloud::Protocol::MessageType::AuthenticateRequest);
        QCOMPARE(result_B.frame.header.payloadLength, static_cast<quint32>(5));
        QCOMPARE(result_B.frame.header.requestId, static_cast<quint64>(102));
        QCOMPARE(result_B.frame.header.taskId, static_cast<quint64>(0));
        QCOMPARE(result_B.frame.payload, payload_B);
        QCOMPARE(parser.bufferedSize(), static_cast<qsizetype>(0));
        QCOMPARE(result_B.status, MiniCloud::Protocol::FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(result_B.errorCode, MiniCloud::Protocol::ErrorCode::None);
    }
};

QTEST_APPLESS_MAIN(FrameParserTest)
#include "frameparsertest.moc"