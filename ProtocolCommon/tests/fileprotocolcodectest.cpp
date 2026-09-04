#include <QtTest/QTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "fileprotocol.h"
#include "frameparser.h"
#include "protocolcodec.h"
#include "protocoltypes.h"

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
};

QTEST_MAIN(FileProtocolCodecTest)
#include "fileprotocolcodectest.moc"
