#include <QtTest/QTest>

#include "errorresponse.h"

class ErrorResponseCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void errorResponse_validData_roundTripsSuccessfully()
    {
        MiniCloud::Protocol::ErrorResponseData originalData;
        originalData.errorCode = MiniCloud::Protocol::ErrorCode::AuthenticationFailed;
        originalData.message = QStringLiteral("Authentication token is invalid.");
        originalData.details.insert(QStringLiteral("retry after"), 30);
        originalData.details.insert(QStringLiteral("reason"), QStringLiteral("het han-token"));

        const MiniCloud::Protocol::ErrorResponseEncodeResult encodeResult = MiniCloud::Protocol::serializeErrorResponse(originalData);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Success);
        QVERIFY(!encodeResult.payload.isEmpty());
        QCOMPARE(encodeResult.errorMessage, QString());

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(encodeResult.payload);
        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Success);
        QCOMPARE(decodeResult.errorMessage, QString());
        QCOMPARE(decodeResult.data.errorCode, originalData.errorCode);
        QCOMPARE(decodeResult.data.message, originalData.message);
        QCOMPARE(decodeResult.data.details, originalData.details);
    }

    void serializeErrorResponse_noneErrorCode_returnsFailed()
    {

        MiniCloud::Protocol::ErrorResponseData data;
        data.errorCode = MiniCloud::Protocol::ErrorCode::None;
        data.message = QStringLiteral("This error response has an invalid error code.");
        data.details.insert(QStringLiteral("reason"), QStringLiteral(""));

        const MiniCloud::Protocol::ErrorResponseEncodeResult encodeResult = MiniCloud::Protocol::serializeErrorResponse(data);

        QCOMPARE(encodeResult.status, MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Failed);
        QVERIFY(encodeResult.payload.isEmpty());
        QVERIFY(!encodeResult.errorMessage.isEmpty());
    }

    void deserializeErrorResponse_malformedJson_returnsFailed()
    {
        const QByteArray malformedJson = R"({"errorCode": 1, "message": "Malformed JSON", "details": {)";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(malformedJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_jsonArray_returnsFailed()
    {

        const QByteArray jsonArray = R"([{"errorCode": 1, "message": "JSON Array", "details": [])";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(jsonArray);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_missingErrorCode_returnsFailed()
    {

        const QByteArray missingErrorCodeJson = R"({"message": "Missing error code", "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(missingErrorCodeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_errorCodeWrongType_returnsFailed()
    {
        
        const QByteArray errorCodeWrongTypeJson = R"({"errorCode": "not an integer", "message": "Error code wrong type", "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(errorCodeWrongTypeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_unknownErrorCode_returnsFailed()
    {
        const QByteArray unknownErrorCodeJson = R"({"errorCode": 9999, "message": "Unknown error code", "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(unknownErrorCodeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_noneErrorCode_returnsFailed()
    { 
        
        const QByteArray noneErrorCodeJson = R"({"errorCode": 0, "message": "None error code", "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(noneErrorCodeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);

    }

    void deserializeErrorResponse_missingMessage_returnsFailed()
    {
        const QByteArray missingMessageJson = R"({"errorCode": 1, "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(missingMessageJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_messageWrongType_returnsFailed()
    {
        const QByteArray messageWrongTypeJson = R"({"errorCode": 1, "message": 123, "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(messageWrongTypeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }

    void deserializeErrorResponse_missingDetails_returnsFailed()
    {
        const QByteArray missingDetailsJson = R"({"errorCode": 1, "message": "Missing details"})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(missingDetailsJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }
    void deserializeErrorResponse_detailsWrongType_returnsFailed()
    {
        const QByteArray detailsWrongTypeJson = R"({"errorCode": 1, "message": "Details wrong type", "details": "not an object"})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(detailsWrongTypeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }
    
    void deserializeErrorResponse_fractionalErrorCode_returnsFailed()
    {
        const QByteArray fractionalErrorCodeJson = R"({"errorCode": 1.5, "message": "Fractional error code", "details": {}})";

        const MiniCloud::Protocol::ErrorResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeErrorResponse(fractionalErrorCodeJson);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Failed);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
        QVERIFY(decodeResult.data.errorCode == MiniCloud::Protocol::ErrorCode::None);
    }
    
    void serializeErrorResponse_unknownErrorCode_returnsFailed()
    {
        MiniCloud::Protocol::ErrorResponseData data;
        data.errorCode = static_cast<MiniCloud::Protocol::ErrorCode>(9999);
        data.message = QStringLiteral("This error response has an unknown error code.");
        data.details.insert(QStringLiteral("reason"), QStringLiteral(""));

        const MiniCloud::Protocol::ErrorResponseEncodeResult encodeResult = MiniCloud::Protocol::serializeErrorResponse(data);

        QCOMPARE(encodeResult.status, MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Failed);
        QVERIFY(encodeResult.payload.isEmpty());
        QVERIFY(!encodeResult.errorMessage.isEmpty());
    }
};

QTEST_MAIN(ErrorResponseCodecTest)
#include "errorresponsetest.moc"
