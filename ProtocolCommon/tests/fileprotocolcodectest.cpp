#include <QtTest/QTest>

#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <limits>

#include "fileprotocol.h"
#include "frameparser.h"
#include "protocolcodec.h"
#include "protocolconstants.h"
#include "protocoltypes.h"

namespace
{
    enum class MutationOperation : int
    {
        Rename,
        Move,
        Delete
    };

    enum class TransferControlOperation : int
    {
        UploadStart,
        UploadReady,
        DownloadRequest,
        DownloadStart
    };

    QByteArray makeRawChunkPayload(quint64 offset, const QByteArray &bytes)
    {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream.setVersion(QDataStream::Qt_6_0);
        stream << offset;

        payload.append(bytes);
        return payload;
    }
}

class FileProtocolCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void browseRequest_rootPath_roundTripsThroughFrame()
    {
        using namespace MiniCloud::Protocol;

        const BrowseRequestData original{QStringLiteral("/")};

        const FileProtocolEncodeResult encodedPayload = serializeBrowseRequest(original);

        QCOMPARE(encodedPayload.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(!encodedPayload.payload.isEmpty());
        QVERIFY(encodedPayload.errorMessage.isEmpty());

        constexpr RequestId requestId = 42;
        constexpr TaskId taskId = 0;

        const FrameEncodeResult encodedFrame = serializeFrame(
            MessageType::BrowseRequest,
            requestId,
            taskId,
            encodedPayload.payload);

        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);

        FrameParser parser;
        parser.appendData(encodedFrame.encodedFrame);
        const FrameParser::FrameParseResult parsed = parser.tryTakeFrame();

        QCOMPARE(parsed.status, FrameParser::FrameParseStatus::FrameReady);

        QCOMPARE(parsed.frame.header.messageType, MessageType::BrowseRequest);
        QCOMPARE(parsed.frame.header.requestId, requestId);
        QCOMPARE(parsed.frame.header.taskId, TaskId{0});

        const BrowseRequestDecodeResult decoded = deserializeBrowseRequest(parsed.frame.payload);

        QCOMPARE(decoded.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/"));
        QVERIFY(decoded.errorMessage.isEmpty());
    }

    void browseRequest_pathLengthLimit_isMeasuredInUtf8Bytes()
    {
        using namespace MiniCloud::Protocol;

        const QString exactLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes - 1, QLatin1Char('a'));
        QCOMPARE(exactLimitPath.toUtf8().size(), protocolMaxLogicalPathUtf8Bytes);

        const FileProtocolEncodeResult exactEncode = serializeBrowseRequest({exactLimitPath});
        QCOMPARE(exactEncode.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(exactEncode.errorMessage.isEmpty());

        const BrowseRequestDecodeResult exactDecode = deserializeBrowseRequest(exactEncode.payload);
        QCOMPARE(exactDecode.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(exactDecode.data.path, exactLimitPath);

        const QString overLimitAsciiPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('a'));
        QVERIFY(overLimitAsciiPath.toUtf8().size() > protocolMaxLogicalPathUtf8Bytes);

        const FileProtocolEncodeResult overAsciiEncode = serializeBrowseRequest({overLimitAsciiPath});
        QCOMPARE(overAsciiEncode.status, FileProtocolEncodeResult::Status::Failed);
        QVERIFY(overAsciiEncode.payload.isEmpty());
        QVERIFY(!overAsciiEncode.errorMessage.isEmpty());

        QJsonObject overLimitObject;
        overLimitObject.insert(QStringLiteral("path"), overLimitAsciiPath);
        const QByteArray overLimitPayload = QJsonDocument(overLimitObject).toJson(QJsonDocument::Compact);

        const BrowseRequestDecodeResult overAsciiDecode = deserializeBrowseRequest(overLimitPayload);
        QCOMPARE(overAsciiDecode.status, BrowseRequestDecodeResult::Status::Failed);
        QVERIFY(overAsciiDecode.data.path.isEmpty());
        QVERIFY(!overAsciiDecode.errorMessage.isEmpty());

        const QString overLimitUnicodePath = QStringLiteral("/") + QString(512, QChar(0x00E9));
        QVERIFY(overLimitUnicodePath.size() < protocolMaxLogicalPathUtf8Bytes);
        QVERIFY(overLimitUnicodePath.toUtf8().size() > protocolMaxLogicalPathUtf8Bytes);

        const FileProtocolEncodeResult overUnicodeEncode = serializeBrowseRequest({overLimitUnicodePath});
        QCOMPARE(overUnicodeEncode.status, FileProtocolEncodeResult::Status::Failed);
        QVERIFY(overUnicodeEncode.payload.isEmpty());
        QVERIFY(!overUnicodeEncode.errorMessage.isEmpty());
    }

    void browseRequest_invalidPayload_deserializeFailsWithoutData_data()
    {
        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("malformed-json") << QByteArrayLiteral(R"({"path":)");
        QTest::newRow("json-array") << QByteArrayLiteral(R"(["/"])");
        QTest::newRow("json-null") << QByteArrayLiteral(R"(null)");
        QTest::newRow("missing-path") << QByteArrayLiteral(R"({})");
        QTest::newRow("path-null") << QByteArrayLiteral(R"({"path":null})");
        QTest::newRow("path-number") << QByteArrayLiteral(R"({"path":7})");
        QTest::newRow("path-boolean") << QByteArrayLiteral(R"({"path":true})");
        QTest::newRow("empty-path") << QByteArrayLiteral(R"({"path":""})");
        QTest::newRow("whitespace-path") << QByteArrayLiteral(R"({"path":"   "})");
    }

    void browseRequest_invalidPayload_deserializeFailsWithoutData()
    {
        QFETCH(QByteArray, payload);

        const MiniCloud::Protocol::BrowseRequestDecodeResult result = MiniCloud::Protocol::deserializeBrowseRequest(payload);

        QCOMPARE(result.status, MiniCloud::Protocol::BrowseRequestDecodeResult::Status::Failed);

        QVERIFY(result.data.path.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }

    void browseResponse_multipleEntries_roundTripsAllMetadataAndPreservesOrder()
    {
        using namespace MiniCloud::Protocol;

        constexpr quint64 largeFileSize = 9007199254740993ULL;

        BrowseResponseData original;
        original.path = QStringLiteral("/");

        original.entries = {
            {QStringLiteral("Documents"),
             QStringLiteral("/Documents"),
             FileEntryType::Directory,
             0,
             1788424496000ULL},
            {QStringLiteral("large.bin"),
             QStringLiteral("/large.bin"),
             FileEntryType::File,
             largeFileSize,
             1788424596789ULL}};

        const FileProtocolEncodeResult encodedPayload = serializeBrowseResponse(original);

        QCOMPARE(encodedPayload.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(!encodedPayload.payload.isEmpty());
        QVERIFY(encodedPayload.errorMessage.isEmpty());

        constexpr RequestId requestId = 42;
        const FrameEncodeResult encodedFrame = serializeFrame(
            MessageType::BrowseResponse,
            requestId,
            TaskId{0},
            encodedPayload.payload);

        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);

        FrameParser parser;
        parser.appendData(encodedFrame.encodedFrame);
        const auto parsed = parser.tryTakeFrame();

        QCOMPARE(parsed.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsed.frame.header.messageType, MessageType::BrowseResponse);
        QCOMPARE(parsed.frame.header.requestId, requestId);
        QCOMPARE(parsed.frame.header.taskId, TaskId{0});

        const BrowseResponseDecodeResult decoded = deserializeBrowseResponse(parsed.frame.payload);

        QCOMPARE(decoded.status, BrowseResponseDecodeResult::Status::Success);
        QVERIFY(decoded.errorMessage.isEmpty());

        QCOMPARE(decoded.data.path, original.path);
        QCOMPARE(decoded.data.entries.size(), 2);

        const FileEntryData &directory = decoded.data.entries.at(0);

        QCOMPARE(directory.name, QStringLiteral("Documents"));
        QCOMPARE(directory.path, QStringLiteral("/Documents"));
        QCOMPARE(directory.type, FileEntryType::Directory);
        QCOMPARE(directory.sizeBytes, quint64{0});
        QCOMPARE(directory.lastModifiedUtcMs, quint64{1788424496000ULL});

        const FileEntryData &file = decoded.data.entries.at(1);

        QCOMPARE(file.name, QStringLiteral("large.bin"));
        QCOMPARE(file.path, QStringLiteral("/large.bin"));
        QCOMPARE(file.type, FileEntryType::File);
        QCOMPARE(file.sizeBytes, largeFileSize);
        QCOMPARE(file.lastModifiedUtcMs, quint64{1788424596789ULL});
    }

    void browseResponse_invalidPayload_failsWithoutPartialData_data()
    {
        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("malformed-json") << QByteArrayLiteral(R"({"path":"/","entries":[)");

        QTest::newRow("json-array") << QByteArrayLiteral(R"(["/",[]])");

        QTest::newRow("missing-path") << QByteArrayLiteral(R"({"entries":[]})");

        QTest::newRow("empty-path") << QByteArrayLiteral(R"({"path":"","entries":[]})");

        QTest::newRow("path-wrong-type") << QByteArrayLiteral(R"({"path":7,"entries":[]})");

        QTest::newRow("missing-entries") << QByteArrayLiteral(R"({"path":"/"})");

        QTest::newRow("entries-wrong-type") << QByteArrayLiteral(R"({"path":"/","entries":{}})");

        QTest::newRow("entry-not-object") << QByteArrayLiteral(R"({"path":"/","entries":[7]})");

        QTest::newRow("entry-missing-name") << QByteArrayLiteral(
            R"({"path":"/","entries":[{"path":"/a","type":1,"sizeBytes":"1","lastModifiedUtcMs":"1"}]})");

        QTest::newRow("entry-invalid-type") << QByteArrayLiteral(
            R"({"path":"/","entries":[{"name":"a","path":"/a","type":99,"sizeBytes":"1","lastModifiedUtcMs":"1"}]})");

        QTest::newRow("size-json-number") << QByteArrayLiteral(
            R"({"path":"/","entries":[{"name":"a","path":"/a","type":1,"sizeBytes":1,"lastModifiedUtcMs":"1"}]})");

        QTest::newRow("size-invalid-decimal") << QByteArrayLiteral(
            R"({"path":"/","entries":[{"name":"a","path":"/a","type":1,"sizeBytes":"12x","lastModifiedUtcMs":"1"}]})");

        QTest::newRow("size-overflow") << QByteArrayLiteral(
            R"({"path":"/","entries":[{"name":"a","path":"/a","type":1,"sizeBytes":"18446744073709551616","lastModifiedUtcMs":"1"}]})");

        QTest::newRow("modified-time-wrong-type") << QByteArrayLiteral(
            R"({"path":"/","entries":[{"name":"a","path":"/a","type":1,"sizeBytes":"1","lastModifiedUtcMs":1}]})");

        QTest::newRow("partial-list-second-entry-invalid") << QByteArrayLiteral(
            R"({"path":"/","entries":[)"
            R"({"name":"valid","path":"/valid","type":1,"sizeBytes":"10","lastModifiedUtcMs":"20"},)"
            R"({"name":"","path":"/invalid","type":1,"sizeBytes":"10","lastModifiedUtcMs":"20"})"
            R"(]})");
    }

    void browseResponse_invalidPayload_failsWithoutPartialData()
    {
        QFETCH(QByteArray, payload);

        const MiniCloud::Protocol::BrowseResponseDecodeResult result = MiniCloud::Protocol::deserializeBrowseResponse(payload);

        QCOMPARE(result.status, MiniCloud::Protocol::BrowseResponseDecodeResult::Status::Failed);

        QVERIFY(result.data.path.isEmpty());
        QVERIFY(result.data.entries.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }

    void browsePayload_extraFields_areIgnored()
    {
        using namespace MiniCloud::Protocol;

        const QByteArray requestPayload = QByteArrayLiteral(R"({"path":"/Documents","futureField":true,"schemaHint":2})");
        const BrowseRequestDecodeResult requestResult = deserializeBrowseRequest(requestPayload);

        QCOMPARE(requestResult.status, BrowseRequestDecodeResult::Status::Success);
        QCOMPARE(requestResult.data.path, QStringLiteral("/Documents"));
        QVERIFY(requestResult.errorMessage.isEmpty());

        const QByteArray responsePayload = QByteArrayLiteral(
            R"({
            "path":"/Documents",
            "futureRootField":{"enabled":true},
            "entries":[
                {
                    "name":"Reports",
                    "path":"/Documents/Reports",
                    "type":2,
                    "sizeBytes":"0",
                    "lastModifiedUtcMs":"1788424496000",
                    "futureEntryField":"ignored"
                },
                {
                    "name":"report.pdf",
                    "path":"/Documents/report.pdf",
                    "type":1,
                    "sizeBytes":"9007199254740993",
                    "lastModifiedUtcMs":"1788424596789",
                    "checksum":"future-value"
                }
            ]
        })");

        const BrowseResponseDecodeResult responseResult = deserializeBrowseResponse(responsePayload);

        QCOMPARE(responseResult.status, BrowseResponseDecodeResult::Status::Success);
        QCOMPARE(responseResult.data.path, QStringLiteral("/Documents"));
        QCOMPARE(responseResult.data.entries.size(), 2);
        QVERIFY(responseResult.errorMessage.isEmpty());

        const FileEntryData &directory = responseResult.data.entries.at(0);

        QCOMPARE(directory.name, QStringLiteral("Reports"));
        QCOMPARE(directory.path, QStringLiteral("/Documents/Reports"));
        QCOMPARE(directory.type, FileEntryType::Directory);
        QCOMPARE(directory.sizeBytes, quint64{0});
        QCOMPARE(directory.lastModifiedUtcMs, quint64{1788424496000ULL});

        const FileEntryData &file = responseResult.data.entries.at(1);

        QCOMPARE(file.name, QStringLiteral("report.pdf"));
        QCOMPARE(file.path, QStringLiteral("/Documents/report.pdf"));
        QCOMPARE(file.type, FileEntryType::File);
        QCOMPARE(file.sizeBytes, quint64{9007199254740993ULL});
        QCOMPARE(file.lastModifiedUtcMs, quint64{1788424596789ULL});
    }

    void createDirectoryContract_validData_roundTripsRequestAndResponse()
    {
        using namespace MiniCloud::Protocol;

        constexpr RequestId requestId = 84;

        const CreateDirectoryRequestData originalRequest{
            QStringLiteral("/Documents"),
            QStringLiteral("Projects")};
        const FileProtocolEncodeResult requestPayload = serializeCreateDirectoryRequest(originalRequest);
        QCOMPARE(requestPayload.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(requestPayload.errorMessage.isEmpty());

        const FrameEncodeResult requestFrame = serializeFrame(
            MessageType::CreateDirectoryRequest,
            requestId,
            TaskId{0},
            requestPayload.payload);
        QCOMPARE(requestFrame.status, FrameEncodeStatus::Success);

        FrameParser requestParser;
        requestParser.appendData(requestFrame.encodedFrame);
        const auto parsedRequest = requestParser.tryTakeFrame();
        QCOMPARE(parsedRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedRequest.frame.header.messageType, MessageType::CreateDirectoryRequest);
        QCOMPARE(parsedRequest.frame.header.requestId, requestId);
        QCOMPARE(parsedRequest.frame.header.taskId, TaskId{0});

        const CreateDirectoryRequestDecodeResult decodedRequest = deserializeCreateDirectoryRequest(parsedRequest.frame.payload);
        QCOMPARE(decodedRequest.status, CreateDirectoryRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.parentPath, originalRequest.parentPath);
        QCOMPARE(decodedRequest.data.name, originalRequest.name);
        QVERIFY(decodedRequest.errorMessage.isEmpty());

        const FileOperationResponseData originalResponse{QStringLiteral("/Documents/Projects")};

        const FileProtocolEncodeResult responsePayload = serializeFileOperationResponse(originalResponse);
        QCOMPARE(responsePayload.status, FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult responseFrame = serializeFrame(
            MessageType::FileOperationResponse,
            requestId,
            TaskId{0},
            responsePayload.payload);
        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);

        FrameParser responseParser;
        responseParser.appendData(responseFrame.encodedFrame);
        const auto parsedResponse = responseParser.tryTakeFrame();

        QCOMPARE(parsedResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedResponse.frame.header.messageType, MessageType::FileOperationResponse);
        QCOMPARE(parsedResponse.frame.header.requestId, requestId);
        QCOMPARE(parsedResponse.frame.header.taskId, TaskId{0});

        const FileOperationResponseDecodeResult decodedResponse = deserializeFileOperationResponse(parsedResponse.frame.payload);
        QCOMPARE(decodedResponse.status, FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(decodedResponse.data.path, QStringLiteral("/Documents/Projects"));
        QVERIFY(decodedResponse.errorMessage.isEmpty());
    }

    void createDirectoryRequest_fieldLimits_areEnforcedByEncoder_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<QString>("parentPath");
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("shouldSucceed");

        const QString exactPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes - 1, QLatin1Char('p'));
        const QString exactName(protocolMaxFileNameUtf8Bytes, QLatin1Char('n'));

        QTest::newRow("normal") << QStringLiteral("/Documents") << QStringLiteral("Projects") << true;
        QTest::newRow("exact-limits") << exactPath << exactName << true;
        QTest::newRow("empty-parent") << QString() << QStringLiteral("Projects") << false;
        QTest::newRow("blank-parent") << QStringLiteral("   ") << QStringLiteral("Projects") << false;
        QTest::newRow("empty-name") << QStringLiteral("/Documents") << QString() << false;
        QTest::newRow("blank-name") << QStringLiteral("/Documents") << QStringLiteral("   ") << false;
        QTest::newRow("parent-over-limit") << QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p')) << QStringLiteral("Projects") << false;
        QTest::newRow("name-over-limit-ascii") << QStringLiteral("/Documents") << QString(protocolMaxFileNameUtf8Bytes + 1, QLatin1Char('n')) << false;
        QTest::newRow("name-over-limit-unicode") << QStringLiteral("/Documents") << QString(128, QChar(0x00E9)) << false;
    }

    void createDirectoryRequest_fieldLimits_areEnforcedByEncoder()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(QString, parentPath);
        QFETCH(QString, name);
        QFETCH(bool, shouldSucceed);

        const CreateDirectoryRequestData original{parentPath, name};
        const FileProtocolEncodeResult encoded = serializeCreateDirectoryRequest(original);

        if (!shouldSucceed)
        {
            QCOMPARE(encoded.status, FileProtocolEncodeResult::Status::Failed);
            QVERIFY(encoded.payload.isEmpty());
            QVERIFY(!encoded.errorMessage.isEmpty());
            return;
        }

        QCOMPARE(encoded.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(!encoded.payload.isEmpty());
        QVERIFY(encoded.errorMessage.isEmpty());

        const CreateDirectoryRequestDecodeResult decoded = deserializeCreateDirectoryRequest(encoded.payload);

        QCOMPARE(decoded.status, CreateDirectoryRequestDecodeResult::Status::Success);
        QCOMPARE(decoded.data.parentPath, parentPath);
        QCOMPARE(decoded.data.name, name);
        QVERIFY(decoded.errorMessage.isEmpty());
    }

    void createDirectoryRequest_invalidPayload_failsWithoutPartialData_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("malformed-json") << QByteArrayLiteral(R"({"parentPath":"/Documents","name":)");
        QTest::newRow("json-array") << QByteArrayLiteral(R"(["/Documents","Projects"])");
        QTest::newRow("missing-parent") << QByteArrayLiteral(R"({"name":"Projects"})");
        QTest::newRow("parent-wrong-type") << QByteArrayLiteral(R"({"parentPath":7,"name":"Projects"})");
        QTest::newRow("empty-parent") << QByteArrayLiteral(R"({"parentPath":"","name":"Projects"})");
        QTest::newRow("blank-parent") << QByteArrayLiteral(R"({"parentPath":"   ","name":"Projects"})");
        QTest::newRow("missing-name") << QByteArrayLiteral(R"({"parentPath":"/Documents"})");
        QTest::newRow("name-wrong-type") << QByteArrayLiteral(R"({"parentPath":"/Documents","name":false})");
        QTest::newRow("empty-name") << QByteArrayLiteral(R"({"parentPath":"/Documents","name":""})");
        QTest::newRow("blank-name") << QByteArrayLiteral(R"({"parentPath":"/Documents","name":"   "})");

        const QString overLimitParent = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));
        QJsonObject overParentObject;
        overParentObject.insert(QStringLiteral("parentPath"), overLimitParent);
        overParentObject.insert(QStringLiteral("name"), QStringLiteral("Projects"));
        QTest::newRow("parent-over-limit") << QJsonDocument(overParentObject).toJson(QJsonDocument::Compact);

        const QString overLimitName(protocolMaxFileNameUtf8Bytes + 1, QLatin1Char('n'));
        QJsonObject overNameObject;
        overNameObject.insert(QStringLiteral("parentPath"), QStringLiteral("/Documents"));
        overNameObject.insert(QStringLiteral("name"), overLimitName);
        QTest::newRow("name-over-limit") << QJsonDocument(overNameObject).toJson(QJsonDocument::Compact);
    }

    void createDirectoryRequest_invalidPayload_failsWithoutPartialData()
    {
        QFETCH(QByteArray, payload);

        const auto result = MiniCloud::Protocol::deserializeCreateDirectoryRequest(payload);

        QCOMPARE(result.status, MiniCloud::Protocol::CreateDirectoryRequestDecodeResult::Status::Failed);

        QVERIFY(result.data.parentPath.isEmpty());
        QVERIFY(result.data.name.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }

    void fileOperationResponse_pathValidationAndExtraFields_followContract()
    {
        using namespace MiniCloud::Protocol;

        const auto failsCleanly = [](const QByteArray &payload)
        {
            const FileOperationResponseDecodeResult result = deserializeFileOperationResponse(payload);
            return result.status == FileOperationResponseDecodeResult::Status::Failed && result.data.path.isEmpty() && !result.errorMessage.isEmpty();
        };

        const QString exactLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes - 1, QLatin1Char('p'));
        QCOMPARE(exactLimitPath.toUtf8().size(), protocolMaxLogicalPathUtf8Bytes);

        const FileProtocolEncodeResult exactEncode = serializeFileOperationResponse({exactLimitPath});
        QCOMPARE(exactEncode.status, FileProtocolEncodeResult::Status::Success);

        const FileOperationResponseDecodeResult exactDecode = deserializeFileOperationResponse(exactEncode.payload);
        QCOMPARE(exactDecode.status, FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(exactDecode.data.path, exactLimitPath);

        const FileProtocolEncodeResult blankEncode = serializeFileOperationResponse({QStringLiteral("   ")});
        QCOMPARE(blankEncode.status, FileProtocolEncodeResult::Status::Failed);
        QVERIFY(blankEncode.payload.isEmpty());
        QVERIFY(!blankEncode.errorMessage.isEmpty());

        const QString overLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));
        const FileProtocolEncodeResult overLimitEncode = serializeFileOperationResponse({overLimitPath});
        QCOMPARE(overLimitEncode.status, FileProtocolEncodeResult::Status::Failed);
        QVERIFY(overLimitEncode.payload.isEmpty());
        QVERIFY(!overLimitEncode.errorMessage.isEmpty());

        QVERIFY(failsCleanly(QByteArrayLiteral(R"({"path":)")));
        QVERIFY(failsCleanly(QByteArrayLiteral(R"(["/Documents"])")));
        QVERIFY(failsCleanly(QByteArrayLiteral(R"({})")));
        QVERIFY(failsCleanly(QByteArrayLiteral(R"({"path":7})")));
        QVERIFY(failsCleanly(QByteArrayLiteral(R"({"path":"   "})")));

        QJsonObject overLimitObject;
        overLimitObject.insert(QStringLiteral("path"), overLimitPath);

        QVERIFY(failsCleanly(QJsonDocument(overLimitObject).toJson(QJsonDocument::Compact)));

        const QByteArray extraFieldsPayload = QByteArrayLiteral(R"({"path":"/Documents/Projects","futureField":true,"metadata":{"version":2}})");

        const FileOperationResponseDecodeResult extraFieldsDecode = deserializeFileOperationResponse(extraFieldsPayload);

        QCOMPARE(extraFieldsDecode.status, FileOperationResponseDecodeResult::Status::Success);
        QCOMPARE(extraFieldsDecode.data.path, QStringLiteral("/Documents/Projects"));
        QVERIFY(extraFieldsDecode.errorMessage.isEmpty());
    }

    void searchContract_validData_roundTripsRequestAndResponse()
    {
        using namespace MiniCloud::Protocol;

        constexpr RequestId requestId = 105;

        const SearchRequestData originalRequest{QStringLiteral("/Documents"), QStringLiteral("report")};

        const FileProtocolEncodeResult requestPayload = serializeSearchRequest(originalRequest);
        QCOMPARE(requestPayload.status, FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult requestFrame = serializeFrame(
            MessageType::SearchRequest,
            requestId,
            TaskId{0},
            requestPayload.payload);
        QCOMPARE(requestFrame.status, FrameEncodeStatus::Success);
        FrameParser requestParser;
        requestParser.appendData(requestFrame.encodedFrame);
        const auto parsedRequest = requestParser.tryTakeFrame();

        QCOMPARE(parsedRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedRequest.frame.header.messageType, MessageType::SearchRequest);
        QCOMPARE(parsedRequest.frame.header.requestId, requestId);
        QCOMPARE(parsedRequest.frame.header.taskId, TaskId{0});

        const SearchRequestDecodeResult decodedRequest = deserializeSearchRequest(parsedRequest.frame.payload);
        QCOMPARE(decodedRequest.status, SearchRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, originalRequest.path);
        QCOMPARE(decodedRequest.data.query, originalRequest.query);

        SearchResponseData originalResponse;
        originalResponse.path = QStringLiteral("/Documents");

        originalResponse.entries = {
            {QStringLiteral("Annual Report.pdf"),
             QStringLiteral("/Documents/Annual Report.pdf"),
             FileEntryType::File,
             4096,
             1788424496000ULL},

            {QStringLiteral("report-archive"),
             QStringLiteral("/Documents/report-archive"),
             FileEntryType::Directory,
             0,
             1788424596000ULL}};

        const FileProtocolEncodeResult responsePayload = serializeSearchResponse(originalResponse);
        QCOMPARE(responsePayload.status, FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult responseFrame = serializeFrame(
            MessageType::SearchResponse,
            requestId,
            TaskId{0},
            responsePayload.payload);
        QCOMPARE(responseFrame.status, FrameEncodeStatus::Success);

        FrameParser responseParser;
        responseParser.appendData(responseFrame.encodedFrame);
        const auto parsedResponse = responseParser.tryTakeFrame();
        QCOMPARE(parsedResponse.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedResponse.frame.header.messageType, MessageType::SearchResponse);
        QCOMPARE(parsedResponse.frame.header.requestId, requestId);
        QCOMPARE(parsedResponse.frame.header.taskId, TaskId{0});

        const SearchResponseDecodeResult decodedResponse = deserializeSearchResponse(parsedResponse.frame.payload);
        QCOMPARE(decodedResponse.status, SearchResponseDecodeResult::Status::Success);
        QCOMPARE(decodedResponse.data.path, originalResponse.path);
        QCOMPARE(decodedResponse.data.entries.size(), 2);
        QCOMPARE(decodedResponse.data.entries.at(0).name, QStringLiteral("Annual Report.pdf"));
        QCOMPARE(decodedResponse.data.entries.at(1).name, QStringLiteral("report-archive"));
    }

    void searchRequest_fieldLimits_areEnforcedByEncoder_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("query");
        QTest::addColumn<bool>("shouldSucceed");

        const QString exactPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes - 1, QLatin1Char('p'));
        const QString exactQuery(protocolMaxSearchQueryUtf8Bytes, QLatin1Char('q'));

        QTest::newRow("normal") << QStringLiteral("/Documents") << QStringLiteral("report") << true;
        QTest::newRow("query-with-surrounding-whitespace") << QStringLiteral("/Documents") << QStringLiteral(" report ") << true;
        QTest::newRow("exact-limits") << exactPath << exactQuery << true;
        QTest::newRow("empty-path") << QString() << QStringLiteral("report") << false;
        QTest::newRow("blank-path") << QStringLiteral("   ") << QStringLiteral("report") << false;
        QTest::newRow("path-over-limit") << QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p')) << QStringLiteral("report") << false;
        QTest::newRow("empty-query") << QStringLiteral("/Documents") << QString() << false;
        QTest::newRow("blank-query") << QStringLiteral("/Documents") << QStringLiteral("   ") << false;
        QTest::newRow("query-over-limit-ascii") << QStringLiteral("/Documents") << QString(protocolMaxSearchQueryUtf8Bytes + 1, QLatin1Char('q')) << false;
        QTest::newRow("query-over-limit-unicode") << QStringLiteral("/Documents") << QString(128, QChar(0x00E9)) << false;
    }

    void searchRequest_fieldLimits_areEnforcedByEncoder()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(QString, path);
        QFETCH(QString, query);
        QFETCH(bool, shouldSucceed);

        const SearchRequestData original{path, query};
        const FileProtocolEncodeResult encoded = serializeSearchRequest(original);

        if (!shouldSucceed)
        {
            QCOMPARE(encoded.status, FileProtocolEncodeResult::Status::Failed);
            QVERIFY(encoded.payload.isEmpty());
            QVERIFY(!encoded.errorMessage.isEmpty());
            return;
        }

        QCOMPARE(encoded.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(!encoded.payload.isEmpty());
        QVERIFY(encoded.errorMessage.isEmpty());

        const SearchRequestDecodeResult decoded = deserializeSearchRequest(encoded.payload);

        QCOMPARE(decoded.status, SearchRequestDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, path);
        QCOMPARE(decoded.data.query, query);
        QVERIFY(decoded.errorMessage.isEmpty());
    }

    void searchRequest_invalidPayload_failsWithoutPartialData_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("malformed-json") << QByteArrayLiteral(R"({"path":"/Documents","query":)");
        QTest::newRow("json-array") << QByteArrayLiteral(R"(["/Documents","report"])");
        QTest::newRow("missing-path") << QByteArrayLiteral(R"({"query":"report"})");
        QTest::newRow("path-wrong-type") << QByteArrayLiteral(R"({"path":7,"query":"report"})");
        QTest::newRow("blank-path") << QByteArrayLiteral(R"({"path":"   ","query":"report"})");
        QTest::newRow("missing-query") << QByteArrayLiteral(R"({"path":"/Documents"})");
        QTest::newRow("query-wrong-type") << QByteArrayLiteral(R"({"path":"/Documents","query":true})");
        QTest::newRow("empty-query") << QByteArrayLiteral(R"({"path":"/Documents","query":""})");
        QTest::newRow("blank-query") << QByteArrayLiteral(R"({"path":"/Documents","query":"   "})");

        const QString overLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));
        QJsonObject overPathObject;
        overPathObject.insert(QStringLiteral("path"), overLimitPath);
        overPathObject.insert(QStringLiteral("query"), QStringLiteral("report"));
        QTest::newRow("path-over-limit") << QJsonDocument(overPathObject).toJson(QJsonDocument::Compact);

        const QString overLimitQuery(protocolMaxSearchQueryUtf8Bytes + 1, QLatin1Char('q'));
        QJsonObject overQueryObject;
        overQueryObject.insert(QStringLiteral("path"), QStringLiteral("/Documents"));
        overQueryObject.insert(QStringLiteral("query"), overLimitQuery);
        QTest::newRow("query-over-limit") << QJsonDocument(overQueryObject).toJson(QJsonDocument::Compact);
    }

    void searchRequest_invalidPayload_failsWithoutPartialData()
    {
        QFETCH(QByteArray, payload);

        const auto result = MiniCloud::Protocol::deserializeSearchRequest(payload);

        QCOMPARE(result.status, MiniCloud::Protocol::SearchRequestDecodeResult::Status::Failed);

        QVERIFY(result.data.path.isEmpty());
        QVERIFY(result.data.query.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }

    void searchPayload_emptyResultsAndExtraFields_areHandled()
    {
        using namespace MiniCloud::Protocol;

        const SearchResponseData emptyResponse{QStringLiteral("/Documents"), {}};

        const FileProtocolEncodeResult encoded = serializeSearchResponse(emptyResponse);
        QCOMPARE(encoded.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(!encoded.payload.isEmpty());
        QVERIFY(encoded.errorMessage.isEmpty());

        const QJsonDocument encodedDocument = QJsonDocument::fromJson(encoded.payload);
        QVERIFY(encodedDocument.isObject());

        const QJsonObject encodedObject = encodedDocument.object();
        QCOMPARE(encodedObject.value(QStringLiteral("path")).toString(), QStringLiteral("/Documents"));
        QVERIFY(encodedObject.value(QStringLiteral("entries")).isArray());
        QVERIFY(encodedObject.value(QStringLiteral("entries")).toArray().isEmpty());

        const SearchResponseDecodeResult decoded = deserializeSearchResponse(encoded.payload);
        QCOMPARE(decoded.status, SearchResponseDecodeResult::Status::Success);
        QCOMPARE(decoded.data.path, QStringLiteral("/Documents"));
        QVERIFY(decoded.data.entries.isEmpty());
        QVERIFY(decoded.errorMessage.isEmpty());

        const QByteArray requestWithExtraFields = QByteArrayLiteral(R"({"path":"/Documents","query":"report","futureField":true,"recursive":false})");

        const SearchRequestDecodeResult decodedRequest = deserializeSearchRequest(requestWithExtraFields);
        QCOMPARE(decodedRequest.status, SearchRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRequest.data.path, QStringLiteral("/Documents"));
        QCOMPARE(decodedRequest.data.query, QStringLiteral("report"));
        QVERIFY(decodedRequest.errorMessage.isEmpty());

        const QByteArray responseWithExtraFields = QByteArrayLiteral(R"({"path":"/Documents","entries":[],"futureField":{"version":2}})");

        const SearchResponseDecodeResult decodedExtraResponse = deserializeSearchResponse(responseWithExtraFields);
        QCOMPARE(decodedExtraResponse.status, SearchResponseDecodeResult::Status::Success);
        QCOMPARE(decodedExtraResponse.data.path, QStringLiteral("/Documents"));
        QVERIFY(decodedExtraResponse.data.entries.isEmpty());
        QVERIFY(decodedExtraResponse.errorMessage.isEmpty());
    }

    void mutationContracts_validData_roundTripThroughFrames()
    {
        using namespace MiniCloud::Protocol;

        constexpr RequestId renameRequestId = 201;
        const RenameRequestData renameOriginal{QStringLiteral("/Documents/old-name.txt"), QStringLiteral("new-name.txt")};

        const FileProtocolEncodeResult renamePayload = serializeRenameRequest(renameOriginal);
        QCOMPARE(renamePayload.status, FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult renameFrame = serializeFrame(
            MessageType::RenameRequest,
            renameRequestId,
            TaskId{0},
            renamePayload.payload);
        QCOMPARE(renameFrame.status, FrameEncodeStatus::Success);

        FrameParser renameParser;
        renameParser.appendData(renameFrame.encodedFrame);
        const auto parsedRename = renameParser.tryTakeFrame();
        QCOMPARE(parsedRename.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedRename.frame.header.messageType, MessageType::RenameRequest);
        QCOMPARE(parsedRename.frame.header.requestId, renameRequestId);
        QCOMPARE(parsedRename.frame.header.taskId, TaskId{0});

        const RenameRequestDecodeResult decodedRename = deserializeRenameRequest(parsedRename.frame.payload);
        QCOMPARE(decodedRename.status, RenameRequestDecodeResult::Status::Success);
        QCOMPARE(decodedRename.data.path, renameOriginal.path);
        QCOMPARE(decodedRename.data.newName, renameOriginal.newName);

        constexpr RequestId moveRequestId = 202;
        const MoveRequestData moveOriginal{QStringLiteral("/Documents/new-name.txt"), QStringLiteral("/Archive")};

        const FileProtocolEncodeResult movePayload = serializeMoveRequest(moveOriginal);
        QCOMPARE(movePayload.status, FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult moveFrame = serializeFrame(
            MessageType::MoveRequest,
            moveRequestId,
            TaskId{0},
            movePayload.payload);
        QCOMPARE(moveFrame.status, FrameEncodeStatus::Success);

        FrameParser moveParser;
        moveParser.appendData(moveFrame.encodedFrame);
        const auto parsedMove = moveParser.tryTakeFrame();
        QCOMPARE(parsedMove.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedMove.frame.header.messageType, MessageType::MoveRequest);
        QCOMPARE(parsedMove.frame.header.requestId, moveRequestId);
        QCOMPARE(parsedMove.frame.header.taskId, TaskId{0});

        const MoveRequestDecodeResult decodedMove = deserializeMoveRequest(parsedMove.frame.payload);
        QCOMPARE(decodedMove.status, MoveRequestDecodeResult::Status::Success);
        QCOMPARE(decodedMove.data.sourcePath, moveOriginal.sourcePath);
        QCOMPARE(decodedMove.data.destinationDirectoryPath, moveOriginal.destinationDirectoryPath);

        constexpr RequestId deleteRequestId = 203;
        const DeleteRequestData deleteOriginal{QStringLiteral("/Archive/new-name.txt")};

        const FileProtocolEncodeResult deletePayload = serializeDeleteRequest(deleteOriginal);
        QCOMPARE(deletePayload.status, FileProtocolEncodeResult::Status::Success);

        const FrameEncodeResult deleteFrame = serializeFrame(
            MessageType::DeleteRequest,
            deleteRequestId,
            TaskId{0},
            deletePayload.payload);
        QCOMPARE(deleteFrame.status, FrameEncodeStatus::Success);
        FrameParser deleteParser;
        deleteParser.appendData(deleteFrame.encodedFrame);
        const auto parsedDelete = deleteParser.tryTakeFrame();
        QCOMPARE(parsedDelete.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedDelete.frame.header.messageType, MessageType::DeleteRequest);
        QCOMPARE(parsedDelete.frame.header.requestId, deleteRequestId);
        QCOMPARE(parsedDelete.frame.header.taskId, TaskId{0});

        const DeleteRequestDecodeResult decodedDelete = deserializeDeleteRequest(parsedDelete.frame.payload);
        QCOMPARE(decodedDelete.status, DeleteRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDelete.data.path, deleteOriginal.path);
    }

    void mutationRequests_fieldLimits_areEnforcedByEncoder_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<int>("operation");
        QTest::addColumn<QString>("firstPath");
        QTest::addColumn<QString>("secondField");
        QTest::addColumn<bool>("shouldSucceed");

        const QString exactPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes - 1, QLatin1Char('p'));
        const QString overLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));
        const QString exactName(protocolMaxFileNameUtf8Bytes, QLatin1Char('n'));
        const QString overLimitUnicodeName(128, QChar(0x00E9));

        QTest::newRow("rename-normal") << static_cast<int>(MutationOperation::Rename) << QStringLiteral("/Documents/old-name.txt") << QStringLiteral("new-name.txt") << true;
        QTest::newRow("rename-exact-limits") << static_cast<int>(MutationOperation::Rename) << exactPath << exactName << true;
        QTest::newRow("rename-blank-path") << static_cast<int>(MutationOperation::Rename) << QStringLiteral("   ") << QStringLiteral("new-name.txt") << false;
        QTest::newRow("rename-path-over-limit") << static_cast<int>(MutationOperation::Rename) << overLimitPath << QStringLiteral("new-name.txt") << false;
        QTest::newRow("rename-blank-new-name") << static_cast<int>(MutationOperation::Rename) << QStringLiteral("/Documents/old-name.txt") << QStringLiteral("   ") << false;
        QTest::newRow("rename-new-name-over-limit-unicode") << static_cast<int>(MutationOperation::Rename) << QStringLiteral("/Documents/old-name.txt") << overLimitUnicodeName << false;

        QTest::newRow("move-normal") << static_cast<int>(MutationOperation::Move) << QStringLiteral("/Documents/report.pdf") << QStringLiteral("/Archive") << true;
        QTest::newRow("move-source-blank") << static_cast<int>(MutationOperation::Move) << QStringLiteral("   ") << QStringLiteral("/Archive") << false;
        QTest::newRow("move-source-over-limit") << static_cast<int>(MutationOperation::Move) << overLimitPath << QStringLiteral("/Archive") << false;
        QTest::newRow("move-destination-blank") << static_cast<int>(MutationOperation::Move) << QStringLiteral("/Documents/report.pdf") << QStringLiteral("   ") << false;
        QTest::newRow("move-destination-over-limit") << static_cast<int>(MutationOperation::Move) << QStringLiteral("/Documents/report.pdf") << overLimitPath << false;

        QTest::newRow("delete-normal") << static_cast<int>(MutationOperation::Delete) << QStringLiteral("/Documents/report.pdf") << QString() << true;
        QTest::newRow("delete-root-path") << static_cast<int>(MutationOperation::Delete) << QStringLiteral("/") << QString() << true;
        QTest::newRow("delete-blank-path") << static_cast<int>(MutationOperation::Delete) << QStringLiteral("   ") << QString() << false;
        QTest::newRow("delete-path-over-limit") << static_cast<int>(MutationOperation::Delete) << overLimitPath << QString() << false;
    }

    void mutationRequests_fieldLimits_areEnforcedByEncoder()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(int, operation);
        QFETCH(QString, firstPath);
        QFETCH(QString, secondField);
        QFETCH(bool, shouldSucceed);

        FileProtocolEncodeResult result;

        switch (static_cast<MutationOperation>(operation))
        {
        case MutationOperation::Rename:
            result = serializeRenameRequest({firstPath, secondField});
            break;
        case MutationOperation::Move:
            result = serializeMoveRequest({firstPath, secondField});
            break;
        case MutationOperation::Delete:
            result = serializeDeleteRequest({firstPath});
            break;
        default:
            QFAIL("Unknown mutation operation.");
            return;
        }

        QCOMPARE(result.status == FileProtocolEncodeResult::Status::Success, shouldSucceed);

        if (shouldSucceed)
        {
            QVERIFY(result.errorMessage.isEmpty());
            QVERIFY(!result.payload.isEmpty());
        }
        else
        {
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.payload.isEmpty());
        }
    }

    void mutationRequests_invalidPayload_failWithoutPartialData_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<int>("operation");
        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("rename-malformed-json") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"path":"/Documents/report.txt","newName":)");
        QTest::newRow("rename-json-array") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"(["/Documents/report.txt","renamed.txt"])");
        QTest::newRow("rename-missing-path") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"newName":"renamed.txt"})");
        QTest::newRow("rename-path-wrong-type") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"path":7,"newName":"renamed.txt"})");
        QTest::newRow("rename-blank-path") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"path":"   ","newName":"renamed.txt"})");
        QTest::newRow("rename-missing-new-name") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"path":"/Documents/report.txt"})");
        QTest::newRow("rename-new-name-wrong-type") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"path":"/Documents/report.txt","newName":false})");
        QTest::newRow("rename-blank-new-name") << static_cast<int>(MutationOperation::Rename) << QByteArrayLiteral(R"({"path":"/Documents/report.txt","newName":"   "})");

        const QString overLimitNewName(128, QChar(0x00E9));
        QJsonObject overLimitRenameObject;
        overLimitRenameObject.insert(QStringLiteral("path"), QStringLiteral("/Documents/report.txt"));
        overLimitRenameObject.insert(QStringLiteral("newName"), overLimitNewName);
        QTest::newRow("rename-new-name-over-limit") << static_cast<int>(MutationOperation::Rename) << QJsonDocument(overLimitRenameObject).toJson(QJsonDocument::Compact);

        QTest::newRow("move-malformed-json") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"sourcePath":"/Documents/report.txt","destinationDirectoryPath":)");
        QTest::newRow("move-missing-source-path") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Archive"})");
        QTest::newRow("move-source-path-wrong-type") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"sourcePath":7,"destinationDirectoryPath":"/Archive"})");
        QTest::newRow("move-blank-source-path") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"sourcePath":"   ","destinationDirectoryPath":"/Archive"})");
        QTest::newRow("move-missing-destination-directory-path") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"sourcePath":"/Documents/report.txt"})");
        QTest::newRow("move-destination-directory-path-wrong-type") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"sourcePath":"/Documents/report.txt","destinationDirectoryPath":false})");
        QTest::newRow("move-blank-destination-directory-path") << static_cast<int>(MutationOperation::Move) << QByteArrayLiteral(R"({"sourcePath":"/Documents/report.txt","destinationDirectoryPath":"   "})");

        const QString overLimitDestination = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('d'));
        QJsonObject overLimitMoveObject;
        overLimitMoveObject.insert(QStringLiteral("sourcePath"), QStringLiteral("/Documents/report.txt"));
        overLimitMoveObject.insert(QStringLiteral("destinationDirectoryPath"), overLimitDestination);
        QTest::newRow("move-destination-directory-path-over-limit") << static_cast<int>(MutationOperation::Move) << QJsonDocument(overLimitMoveObject).toJson(QJsonDocument::Compact);

        QTest::newRow("delete-malformed-json") << static_cast<int>(MutationOperation::Delete) << QByteArrayLiteral(R"({"path":)");
        QTest::newRow("delete-json-array") << static_cast<int>(MutationOperation::Delete) << QByteArrayLiteral(R"(["/Documents/report.txt"])");
        QTest::newRow("delete-missing-path") << static_cast<int>(MutationOperation::Delete) << QByteArrayLiteral(R"({})");
        QTest::newRow("delete-path-wrong-type") << static_cast<int>(MutationOperation::Delete) << QByteArrayLiteral(R"({"path":7})");
        QTest::newRow("delete-blank-path") << static_cast<int>(MutationOperation::Delete) << QByteArrayLiteral(R"({"path":"   "})");

        const QString overLimitDeletePath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));
        QJsonObject overLimitDeleteObject;
        overLimitDeleteObject.insert(QStringLiteral("path"), overLimitDeletePath);
        QTest::newRow("delete-path-over-limit") << static_cast<int>(MutationOperation::Delete) << QJsonDocument(overLimitDeleteObject).toJson(QJsonDocument::Compact);
    }

    void mutationRequests_invalidPayload_failWithoutPartialData()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(int, operation);
        QFETCH(QByteArray, payload);

        switch (static_cast<MutationOperation>(operation))
        {
        case MutationOperation::Rename:
        {
            const RenameRequestDecodeResult result = deserializeRenameRequest(payload);
            QCOMPARE(result.status, RenameRequestDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.path.isEmpty());
            QVERIFY(result.data.newName.isEmpty());
            break;
        }
        case MutationOperation::Move:
        {
            const MoveRequestDecodeResult result = deserializeMoveRequest(payload);
            QCOMPARE(result.status, MoveRequestDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.sourcePath.isEmpty());
            QVERIFY(result.data.destinationDirectoryPath.isEmpty());
            break;
        }
        case MutationOperation::Delete:
        {
            const DeleteRequestDecodeResult result = deserializeDeleteRequest(payload);
            QCOMPARE(result.status, DeleteRequestDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.path.isEmpty());
            break;
        }
        default:
            QFAIL("Unknown mutation operation.");
            break;
        }
    }

    void fileOperationRequestPayloads_extraFields_areIgnored()
    {
        using namespace MiniCloud::Protocol;

        const CreateDirectoryRequestDecodeResult createResult = deserializeCreateDirectoryRequest(
            QByteArrayLiteral(R"({"parentPath":"/Documents","name":"Projects","color":"blue"})"));

        QCOMPARE(createResult.status, CreateDirectoryRequestDecodeResult::Status::Success);
        QVERIFY(createResult.errorMessage.isEmpty());
        QCOMPARE(createResult.data.parentPath, QStringLiteral("/Documents"));
        QCOMPARE(createResult.data.name, QStringLiteral("Projects"));

        const RenameRequestDecodeResult renameResult = deserializeRenameRequest(
            QByteArrayLiteral(R"({"path":"/Documents/old.txt","newName":"new.txt","overwrite":false})"));

        QCOMPARE(renameResult.status, RenameRequestDecodeResult::Status::Success);
        QVERIFY(renameResult.errorMessage.isEmpty());
        QCOMPARE(renameResult.data.path, QStringLiteral("/Documents/old.txt"));
        QCOMPARE(renameResult.data.newName, QStringLiteral("new.txt"));

        const MoveRequestDecodeResult moveResult = deserializeMoveRequest(
            QByteArrayLiteral(R"({"sourcePath":"/Documents/new.txt","destinationDirectoryPath":"/Archive","futureOption":{"priority":1}})"));

        QCOMPARE(moveResult.status, MoveRequestDecodeResult::Status::Success);
        QVERIFY(moveResult.errorMessage.isEmpty());
        QCOMPARE(moveResult.data.sourcePath, QStringLiteral("/Documents/new.txt"));
        QCOMPARE(moveResult.data.destinationDirectoryPath, QStringLiteral("/Archive"));

        const DeleteRequestDecodeResult deleteResult = deserializeDeleteRequest(
            QByteArrayLiteral(R"({"path":"/Archive/new.txt","recursive":true})"));

        QCOMPARE(deleteResult.status, DeleteRequestDecodeResult::Status::Success);
        QVERIFY(deleteResult.errorMessage.isEmpty());
        QCOMPARE(deleteResult.data.path, QStringLiteral("/Archive/new.txt"));
    }

    void fileChunk_binaryPayload_roundTripsWithOffsetAndBoundarySizes_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<quint64>("offset");
        QTest::addColumn<QByteArray>("bytes");

        QTest::newRow("empty-offset-data") << quint64{0} << QByteArrayLiteral("hello");
        QTest::newRow("non-zero-offset-data") << quint64{65536} << QByteArrayLiteral("\x01\0\xFF\x7F");
        QTest::newRow("exact-64-kib-data") << quint64{131072} << QByteArray(static_cast<qsizetype>(protocolMaxFileChunkDataBytes), '\xA5');
    }

    void fileChunk_binaryPayload_roundTripsWithOffsetAndBoundarySizes()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(quint64, offset);
        QFETCH(QByteArray, bytes);

        const FileChunkData data{offset, bytes};

        const FileProtocolEncodeResult encoded = serializeFileChunk(data);

        QCOMPARE(encoded.status, FileProtocolEncodeResult::Status::Success);
        constexpr RequestId requestId = 401;

        const FrameEncodeResult encodedFrame = serializeFrame(
            MessageType::FileChunk,
            requestId,
            TaskId{0},
            encoded.payload);
        QCOMPARE(encodedFrame.status, FrameEncodeStatus::Success);
        FrameParser parser;
        parser.appendData(encodedFrame.encodedFrame);
        const FrameParser::FrameParseResult parsed = parser.tryTakeFrame();
        QCOMPARE(parsed.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsed.frame.header.messageType, MessageType::FileChunk);
        QCOMPARE(parsed.frame.header.requestId, requestId);
        QCOMPARE(parsed.frame.header.taskId, TaskId{0});

        const FileChunkDecodeResult decoded = deserializeFileChunk(parsed.frame.payload);
        QCOMPARE(decoded.status, FileChunkDecodeResult::Status::Success);
        QCOMPARE(decoded.data.offset, data.offset);
        QCOMPARE(decoded.data.bytes, data.bytes);
    }

    void fileChunk_invalidPayload_failsWithoutPartialData_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("missing-offset-byte") << QByteArray(7, '\0');
        QTest::newRow("empty-chunk-data") << makeRawChunkPayload(42, QByteArray());
        QTest::newRow("chunk-data-over-64-kib") << makeRawChunkPayload(42, QByteArray(static_cast<qsizetype>(protocolMaxFileChunkDataBytes) + 1, 'x'));
    }

    void fileChunk_invalidPayload_failsWithoutPartialData()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(QByteArray, payload);

        const FileChunkDecodeResult result = deserializeFileChunk(payload);
        QCOMPARE(result.status, FileChunkDecodeResult::Status::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.data.offset, quint64{0});
        QVERIFY(result.data.bytes.isEmpty());
    }

    void fileChunk_encoder_rejectsEmptyAndOversizeData_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<QByteArray>("bytes");

        QTest::newRow("empty-data") << QByteArray();

        QTest::newRow("data-over-64-kib") << QByteArray(static_cast<qsizetype>(protocolMaxFileChunkDataBytes) + 1, 'x');
    }

    void fileChunk_encoder_rejectsEmptyAndOversizeData()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(QByteArray, bytes);

        const FileProtocolEncodeResult result = serializeFileChunk(FileChunkData{42, bytes});

        QCOMPARE(result.status, FileProtocolEncodeResult::Status::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.payload.isEmpty());
    }

    void transferControlContracts_validData_roundTripThroughFrames()
    {
        using namespace MiniCloud::Protocol;

        constexpr quint64 largeSize = 9007199254740993ULL;
        constexpr RequestId uploadRequestId = 301;
        constexpr RequestId downloadRequestId = 302;

        const UploadStartRequestData uploadStart{QStringLiteral("/Documents"), QStringLiteral("archive.bin"), largeSize};

        const FileProtocolEncodeResult encodedUploadStart = serializeUploadStartRequest(uploadStart);
        QCOMPARE(encodedUploadStart.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(encodedUploadStart.errorMessage.isEmpty());

        const FrameEncodeResult encodedUploadStartFrame = serializeFrame(
            MessageType::UploadStartRequest,
            uploadRequestId,
            TaskId{0},
            encodedUploadStart.payload);
        QCOMPARE(encodedUploadStartFrame.status, FrameEncodeStatus::Success);
        FrameParser uploadStartParser;
        uploadStartParser.appendData(encodedUploadStartFrame.encodedFrame);
        const FrameParser::FrameParseResult parsedUploadStart = uploadStartParser.tryTakeFrame();
        QCOMPARE(parsedUploadStart.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedUploadStart.frame.header.messageType, MessageType::UploadStartRequest);
        QCOMPARE(parsedUploadStart.frame.header.requestId, uploadRequestId);
        QCOMPARE(parsedUploadStart.frame.header.taskId, TaskId{0});

        const UploadStartRequestDecodeResult decodedUploadStart = deserializeUploadStartRequest(parsedUploadStart.frame.payload);
        QCOMPARE(decodedUploadStart.status, UploadStartRequestDecodeResult::Status::Success);
        QCOMPARE(decodedUploadStart.data.destinationDirectoryPath, uploadStart.destinationDirectoryPath);
        QCOMPARE(decodedUploadStart.data.fileName, uploadStart.fileName);
        QCOMPARE(decodedUploadStart.data.totalSizeBytes, largeSize);
        QVERIFY(decodedUploadStart.errorMessage.isEmpty());

        const UploadReadyResponseData uploadReady{QStringLiteral("/Documents/archive.bin")};
        const FileProtocolEncodeResult encodedUploadReady = serializeUploadReadyResponse(uploadReady);
        QCOMPARE(encodedUploadReady.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(encodedUploadReady.errorMessage.isEmpty());

        const FrameEncodeResult encodedUploadReadyFrame = serializeFrame(
            MessageType::UploadReadyResponse,
            uploadRequestId,
            TaskId{0},
            encodedUploadReady.payload);
        QCOMPARE(encodedUploadReadyFrame.status, FrameEncodeStatus::Success);

        FrameParser uploadReadyParser;
        uploadReadyParser.appendData(encodedUploadReadyFrame.encodedFrame);
        const FrameParser::FrameParseResult parsedUploadReady = uploadReadyParser.tryTakeFrame();
        QCOMPARE(parsedUploadReady.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedUploadReady.frame.header.messageType, MessageType::UploadReadyResponse);
        QCOMPARE(parsedUploadReady.frame.header.requestId, uploadRequestId);
        QCOMPARE(parsedUploadReady.frame.header.taskId, TaskId{0});

        const UploadReadyResponseDecodeResult decodedUploadReady = deserializeUploadReadyResponse(parsedUploadReady.frame.payload);
        QCOMPARE(decodedUploadReady.status, UploadReadyResponseDecodeResult::Status::Success);
        QCOMPARE(decodedUploadReady.data.path, uploadReady.path);
        QVERIFY(decodedUploadReady.errorMessage.isEmpty());

        const DownloadRequestData downloadRequest{QStringLiteral("/Documents/archive.bin")};
        const FileProtocolEncodeResult encodedDownloadRequest = serializeDownloadRequest(downloadRequest);
        QCOMPARE(encodedDownloadRequest.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(encodedDownloadRequest.errorMessage.isEmpty());

        const FrameEncodeResult encodedDownloadRequestFrame = serializeFrame(
            MessageType::DownloadRequest,
            downloadRequestId,
            TaskId{0},
            encodedDownloadRequest.payload);
        QCOMPARE(encodedDownloadRequestFrame.status, FrameEncodeStatus::Success);

        FrameParser downloadRequestParser;
        downloadRequestParser.appendData(encodedDownloadRequestFrame.encodedFrame);
        const FrameParser::FrameParseResult parsedDownloadRequest = downloadRequestParser.tryTakeFrame();
        QCOMPARE(parsedDownloadRequest.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedDownloadRequest.frame.header.messageType, MessageType::DownloadRequest);
        QCOMPARE(parsedDownloadRequest.frame.header.requestId, downloadRequestId);
        QCOMPARE(parsedDownloadRequest.frame.header.taskId, TaskId{0});

        const DownloadRequestDecodeResult decodedDownloadRequest = deserializeDownloadRequest(parsedDownloadRequest.frame.payload);
        QCOMPARE(decodedDownloadRequest.status, DownloadRequestDecodeResult::Status::Success);
        QCOMPARE(decodedDownloadRequest.data.path, downloadRequest.path);
        QVERIFY(decodedDownloadRequest.errorMessage.isEmpty());

        const DownloadStartResponseData downloadStart{QStringLiteral("/Documents/archive.bin"), largeSize};
        const FileProtocolEncodeResult encodedDownloadStart = serializeDownloadStartResponse(downloadStart);
        QCOMPARE(encodedDownloadStart.status, FileProtocolEncodeResult::Status::Success);
        QVERIFY(encodedDownloadStart.errorMessage.isEmpty());

        const FrameEncodeResult encodedDownloadStartFrame = serializeFrame(
            MessageType::DownloadStartResponse,
            downloadRequestId,
            TaskId{0},
            encodedDownloadStart.payload);
        QCOMPARE(encodedDownloadStartFrame.status, FrameEncodeStatus::Success);

        FrameParser downloadStartParser;
        downloadStartParser.appendData(encodedDownloadStartFrame.encodedFrame);
        const FrameParser::FrameParseResult parsedDownloadStart = downloadStartParser.tryTakeFrame();
        QCOMPARE(parsedDownloadStart.status, FrameParser::FrameParseStatus::FrameReady);
        QCOMPARE(parsedDownloadStart.frame.header.messageType, MessageType::DownloadStartResponse);
        QCOMPARE(parsedDownloadStart.frame.header.requestId, downloadRequestId);
        QCOMPARE(parsedDownloadStart.frame.header.taskId, TaskId{0});

        const DownloadStartResponseDecodeResult decodedDownloadStart = deserializeDownloadStartResponse(parsedDownloadStart.frame.payload);
        QCOMPARE(decodedDownloadStart.status, DownloadStartResponseDecodeResult::Status::Success);
        QCOMPARE(decodedDownloadStart.data.path, downloadStart.path);
        QCOMPARE(decodedDownloadStart.data.totalSizeBytes, largeSize);
        QVERIFY(decodedDownloadStart.errorMessage.isEmpty());
    }

    void transferControl_fieldLimits_areEnforcedByEncoder_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<int>("operation");
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<quint64>("totalSizeBytes");
        QTest::addColumn<bool>("shouldSucceed");

        const int uploadStart = static_cast<int>(TransferControlOperation::UploadStart);
        const int uploadReady = static_cast<int>(TransferControlOperation::UploadReady);
        const int downloadRequest = static_cast<int>(TransferControlOperation::DownloadRequest);
        const int downloadStart = static_cast<int>(TransferControlOperation::DownloadStart);

        const quint64 maximumQuint64 = std::numeric_limits<quint64>::max();
        const QString exactLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes - 1, QLatin1Char('p'));
        const QString exactLimitFileName(protocolMaxFileNameUtf8Bytes, QLatin1Char('n'));
        const QString overLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));

        QTest::newRow("upload-start-normal") << uploadStart << QStringLiteral("/Documents") << QStringLiteral("archive.bin") << quint64{42} << true;
        QTest::newRow("upload-start-zero-byte-file") << uploadStart << QStringLiteral("/Documents") << QStringLiteral("empty.bin") << quint64{0} << true;
        QTest::newRow("upload-start-maximum-quint64") << uploadStart << QStringLiteral("/Documents") << QStringLiteral("large.bin") << maximumQuint64 << true;
        QTest::newRow("upload-start-exact-path-and-name-limits") << uploadStart << exactLimitPath << exactLimitFileName << quint64{1} << true;
        QTest::newRow("upload-start-blank-destination") << uploadStart << QStringLiteral("   ") << QStringLiteral("archive.bin") << quint64{1} << false;
        QTest::newRow("upload-start-destination-over-limit") << uploadStart << overLimitPath << QStringLiteral("archive.bin") << quint64{1} << false;
        QTest::newRow("upload-start-blank-file-name") << uploadStart << QStringLiteral("/Documents") << QStringLiteral("   ") << quint64{1} << false;
        QTest::newRow("upload-start-file-name-over-limit-unicode") << uploadStart << QStringLiteral("/Documents") << QString(128, QChar(0x00E9)) << quint64{1} << false;

        QTest::newRow("upload-ready-normal") << uploadReady << QStringLiteral("/Documents/archive.bin") << QString() << quint64{0} << true;
        QTest::newRow("upload-ready-blank-path") << uploadReady << QStringLiteral("   ") << QString() << quint64{0} << false;
        QTest::newRow("upload-ready-path-over-limit") << uploadReady << overLimitPath << QString() << quint64{0} << false;

        QTest::newRow("download-request-normal") << downloadRequest << QStringLiteral("/Documents/archive.bin") << QString() << quint64{0} << true;
        QTest::newRow("download-request-blank-path") << downloadRequest << QStringLiteral("   ") << QString() << quint64{0} << false;
        QTest::newRow("download-request-path-over-limit") << downloadRequest << overLimitPath << QString() << quint64{0} << false;

        QTest::newRow("download-start-normal") << downloadStart << QStringLiteral("/Documents/archive.bin") << QString() << quint64{42} << true;
        QTest::newRow("download-start-maximum-quint64") << downloadStart << QStringLiteral("/Documents/archive.bin") << QString() << maximumQuint64 << true;
        QTest::newRow("download-start-blank-path") << downloadStart << QStringLiteral("   ") << QString() << quint64{1} << false;
        QTest::newRow("download-start-path-over-limit") << downloadStart << overLimitPath << QString() << quint64{1} << false;
    }

    void transferControl_fieldLimits_areEnforcedByEncoder()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(int, operation);
        QFETCH(QString, path);
        QFETCH(QString, fileName);
        QFETCH(quint64, totalSizeBytes);
        QFETCH(bool, shouldSucceed);

        FileProtocolEncodeResult result;

        switch (static_cast<TransferControlOperation>(operation))
        {
        case TransferControlOperation::UploadStart:
            result = serializeUploadStartRequest({path, fileName, totalSizeBytes});
            break;
        case TransferControlOperation::UploadReady:
            result = serializeUploadReadyResponse({path});
            break;
        case TransferControlOperation::DownloadRequest:
            result = serializeDownloadRequest({path});
            break;
        case TransferControlOperation::DownloadStart:
            result = serializeDownloadStartResponse({path, totalSizeBytes});
            break;
        default:
            QFAIL("Unknown transfer control operation.");
            return;
        }

        QCOMPARE(result.status == FileProtocolEncodeResult::Status::Success, shouldSucceed);

        if (shouldSucceed)
        {
            QVERIFY(result.errorMessage.isEmpty());
            QVERIFY(!result.payload.isEmpty());
        }
        else
        {
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.payload.isEmpty());
        }
    }

    void transferControl_invalidPayloads_failWithoutPartialData_data()
    {
        using namespace MiniCloud::Protocol;

        QTest::addColumn<int>("operation");
        QTest::addColumn<QByteArray>("payload");

        const int uploadStart = static_cast<int>(TransferControlOperation::UploadStart);
        const int uploadReady = static_cast<int>(TransferControlOperation::UploadReady);
        const int downloadRequest = static_cast<int>(TransferControlOperation::DownloadRequest);
        const int downloadStart = static_cast<int>(TransferControlOperation::DownloadStart);

        QTest::newRow("upload-malformed-json") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin","totalSizeBytes":)");
        QTest::newRow("upload-json-array") << uploadStart << QByteArrayLiteral(R"(["/Documents","a.bin","0"])");
        QTest::newRow("upload-missing-destination") << uploadStart << QByteArrayLiteral(R"({"fileName":"a.bin","totalSizeBytes":"0"})");
        QTest::newRow("upload-destination-wrong-type") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":7,"fileName":"a.bin","totalSizeBytes":"0"})");
        QTest::newRow("upload-blank-destination") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"   ","fileName":"a.bin","totalSizeBytes":"0"})");
        QTest::newRow("upload-missing-file-name") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","totalSizeBytes":"0"})");
        QTest::newRow("upload-file-name-wrong-type") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":false,"totalSizeBytes":"0"})");
        QTest::newRow("upload-blank-file-name") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"   ","totalSizeBytes":"0"})");
        QTest::newRow("upload-missing-total-size") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin"})");
        QTest::newRow("upload-total-size-json-number") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin","totalSizeBytes":42})");
        QTest::newRow("upload-total-size-empty-string") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin","totalSizeBytes":""})");
        QTest::newRow("upload-total-size-negative") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin","totalSizeBytes":"-1"})");
        QTest::newRow("upload-total-size-decimal") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin","totalSizeBytes":"1.5"})");
        QTest::newRow("upload-total-size-overflow") << uploadStart << QByteArrayLiteral(R"({"destinationDirectoryPath":"/Documents","fileName":"a.bin","totalSizeBytes":"18446744073709551616"})");

        const QString overLimitPath = QStringLiteral("/") + QString(protocolMaxLogicalPathUtf8Bytes, QLatin1Char('p'));
        QJsonObject overLimitPathObject;
        overLimitPathObject.insert(QStringLiteral("path"), overLimitPath);
        const QByteArray overLimitPathPayload = QJsonDocument(overLimitPathObject).toJson(QJsonDocument::Compact);

        QTest::newRow("upload-ready-malformed-json") << uploadReady << QByteArrayLiteral(R"({"path":)");
        QTest::newRow("upload-ready-missing-path") << uploadReady << QByteArrayLiteral(R"({})");
        QTest::newRow("upload-ready-path-wrong-type") << uploadReady << QByteArrayLiteral(R"({"path":7})");
        QTest::newRow("upload-ready-blank-path") << uploadReady << QByteArrayLiteral(R"({"path":"   "})");
        QTest::newRow("upload-ready-path-over-limit") << uploadReady << overLimitPathPayload;

        QTest::newRow("download-malformed-json") << downloadRequest << QByteArrayLiteral(R"({"path":)");
        QTest::newRow("download-json-array") << downloadRequest << QByteArrayLiteral(R"(["/Documents/archive.bin"])");
        QTest::newRow("download-missing-path") << downloadRequest << QByteArrayLiteral(R"({})");
        QTest::newRow("download-path-wrong-type") << downloadRequest << QByteArrayLiteral(R"({"path":true})");
        QTest::newRow("download-blank-path") << downloadRequest << QByteArrayLiteral(R"({"path":"   "})");
        QTest::newRow("download-path-over-limit") << downloadRequest << overLimitPathPayload;

        QTest::newRow("download-start-malformed-json") << downloadStart << QByteArrayLiteral(R"({"path":"/Documents/archive.bin","totalSizeBytes":)");
        QTest::newRow("download-start-missing-path") << downloadStart << QByteArrayLiteral(R"({"totalSizeBytes":"0"})");
        QTest::newRow("download-start-blank-path") << downloadStart << QByteArrayLiteral(R"({"path":"   ","totalSizeBytes":"0"})");
        QTest::newRow("download-start-missing-total-size") << downloadStart << QByteArrayLiteral(R"({"path":"/Documents/archive.bin"})");
        QTest::newRow("download-start-total-size-json-number") << downloadStart << QByteArrayLiteral(R"({"path":"/Documents/archive.bin","totalSizeBytes":42})");
        QTest::newRow("download-start-total-size-negative") << downloadStart << QByteArrayLiteral(R"({"path":"/Documents/archive.bin","totalSizeBytes":"-1"})");
        QTest::newRow("download-start-total-size-overflow") << downloadStart << QByteArrayLiteral(R"({"path":"/Documents/archive.bin","totalSizeBytes":"18446744073709551616"})");
    }

    void transferControl_invalidPayloads_failWithoutPartialData()
    {
        using namespace MiniCloud::Protocol;

        QFETCH(int, operation);
        QFETCH(QByteArray, payload);

        switch (static_cast<TransferControlOperation>(operation))
        {
        case TransferControlOperation::UploadStart:
        {
            const UploadStartRequestDecodeResult result = deserializeUploadStartRequest(payload);
            QCOMPARE(result.status, UploadStartRequestDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.destinationDirectoryPath.isEmpty());
            QVERIFY(result.data.fileName.isEmpty());
            QCOMPARE(result.data.totalSizeBytes, quint64{0});
            break;
        }
        case TransferControlOperation::UploadReady:
        {
            const UploadReadyResponseDecodeResult result = deserializeUploadReadyResponse(payload);
            QCOMPARE(result.status, UploadReadyResponseDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.path.isEmpty());
            break;
        }
        case TransferControlOperation::DownloadRequest:
        {
            const DownloadRequestDecodeResult result = deserializeDownloadRequest(payload);
            QCOMPARE(result.status, DownloadRequestDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.path.isEmpty());
            break;
        }
        case TransferControlOperation::DownloadStart:
        {
            const DownloadStartResponseDecodeResult result = deserializeDownloadStartResponse(payload);
            QCOMPARE(result.status, DownloadStartResponseDecodeResult::Status::Failed);
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.data.path.isEmpty());
            QCOMPARE(result.data.totalSizeBytes, quint64{0});
            break;
        }
        default:
            QFAIL("Unknown transfer control operation.");
            return;
        }
    }

    void transferControlPayloads_extraFields_areIgnored()
    {
        using namespace MiniCloud::Protocol;

        const UploadStartRequestDecodeResult uploadStart = deserializeUploadStartRequest(
            QByteArrayLiteral(
                R"({"destinationDirectoryPath":"/Documents",)"
                R"("fileName":"archive.bin",)"
                R"("totalSizeBytes":"42",)"
                R"("checksum":"deferred"})"));

        QCOMPARE(uploadStart.status, UploadStartRequestDecodeResult::Status::Success);
        QVERIFY(uploadStart.errorMessage.isEmpty());
        QCOMPARE(uploadStart.data.destinationDirectoryPath, QStringLiteral("/Documents"));
        QCOMPARE(uploadStart.data.fileName, QStringLiteral("archive.bin"));
        QCOMPARE(uploadStart.data.totalSizeBytes, quint64{42});

        const UploadReadyResponseDecodeResult uploadReady = deserializeUploadReadyResponse(QByteArrayLiteral(R"({"path":"/Documents/archive.bin",)"
                                                                                                             R"("temporaryUploadId":"not-used-in-mvp"})"));

        QCOMPARE(uploadReady.status, UploadReadyResponseDecodeResult::Status::Success);
        QVERIFY(uploadReady.errorMessage.isEmpty());
        QCOMPARE(uploadReady.data.path, QStringLiteral("/Documents/archive.bin"));

        const DownloadRequestDecodeResult downloadRequest = deserializeDownloadRequest(QByteArrayLiteral(
            R"({"path":"/Documents/archive.bin",)"
            R"("resumeOffset":"0"})"));

        QCOMPARE(downloadRequest.status, DownloadRequestDecodeResult::Status::Success);
        QVERIFY(downloadRequest.errorMessage.isEmpty());
        QCOMPARE(downloadRequest.data.path, QStringLiteral("/Documents/archive.bin"));

        const DownloadStartResponseDecodeResult downloadStart = deserializeDownloadStartResponse(QByteArrayLiteral(
            R"({"path":"/Documents/archive.bin",)"
            R"("totalSizeBytes":"42",)"
            R"("checksum":"deferred"})"));

        QCOMPARE(downloadStart.status, DownloadStartResponseDecodeResult::Status::Success);
        QVERIFY(downloadStart.errorMessage.isEmpty());
        QCOMPARE(downloadStart.data.path, QStringLiteral("/Documents/archive.bin"));
        QCOMPARE(downloadStart.data.totalSizeBytes, quint64{42});
    }
};

QTEST_MAIN(FileProtocolCodecTest)
#include "fileprotocolcodectest.moc"
