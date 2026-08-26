#include <QtTest/QTest>
#include "authentication.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

class AuthenticationCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void authenticateRequest_validData_roundTripsSuccessfully()
    {
        MiniCloud::Protocol::AuthenticateRequestData originalData;
        originalData.productKey = QStringLiteral("product-key-123");
        originalData.deviceId = QStringLiteral("device-id-456");

        const MiniCloud::Protocol::AuthenticationEncodeResult encodeResult = MiniCloud::Protocol::serializeAuthenticateRequest(originalData);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::AuthenticationEncodeResult::Status::Success);
        QVERIFY(!encodeResult.payload.isEmpty());
        QVERIFY(encodeResult.errorMessage.isEmpty());

        const MiniCloud::Protocol::AuthenticateRequestDecodeResult decodeResult = MiniCloud::Protocol::deserializeAuthenticateRequest(encodeResult.payload);
        QCOMPARE(decodeResult.status, MiniCloud::Protocol::AuthenticateRequestDecodeResult::Status::Success);

        QCOMPARE(decodeResult.data.productKey, originalData.productKey);
        QCOMPARE(decodeResult.data.deviceId, originalData.deviceId);
    }

    void authenticateResponse_validStatuses_roundTrip_data()
    {
        QTest::addColumn<MiniCloud::Protocol::AuthenticationStatus>("status");
        QTest::newRow("Valid") << MiniCloud::Protocol::AuthenticationStatus::Valid;
        QTest::newRow("InvalidKey") << MiniCloud::Protocol::AuthenticationStatus::InvalidKey;
        QTest::newRow("Disabled") << MiniCloud::Protocol::AuthenticationStatus::Disabled;
        QTest::newRow("DeviceMismatch") << MiniCloud::Protocol::AuthenticationStatus::DeviceMismatch;
    }

    void authenticateResponse_validStatuses_roundTrip()
    {
        QFETCH(MiniCloud::Protocol::AuthenticationStatus, status);

        MiniCloud::Protocol::AuthenticateResponseData originalData;
        originalData.status = status;

        const MiniCloud::Protocol::AuthenticationEncodeResult encodeResult = MiniCloud::Protocol::serializeAuthenticateResponse(originalData);

        QCOMPARE(
            encodeResult.status,
            MiniCloud::Protocol::AuthenticationEncodeResult::Status::Success);

        QVERIFY(!encodeResult.payload.isEmpty());
        QVERIFY(encodeResult.errorMessage.isEmpty());

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(encodeResult.payload, &parseError);
        QVERIFY(parseError.error == QJsonParseError::NoError);
        QVERIFY(document.isObject());

        const QJsonObject jsonObject = document.object();
        QVERIFY(jsonObject.contains(QStringLiteral("status")));

        const QJsonValue statusValue = jsonObject.value(QStringLiteral("status"));
        QVERIFY(statusValue.isDouble());

        QCOMPARE(statusValue.toDouble(), static_cast<double>(static_cast<quint16>(status)));

        const MiniCloud::Protocol::AuthenticateResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeAuthenticateResponse(encodeResult.payload);
        QCOMPARE(decodeResult.status, MiniCloud::Protocol::AuthenticateResponseDecodeResult::Status::Success);
        QCOMPARE(decodeResult.data.status, originalData.status);
        QCOMPARE(decodeResult.errorMessage, QString());
    }

    void authenticateResponse_invalidStatuses_serializeFails_data()
    {
        QTest::addColumn<MiniCloud::Protocol::AuthenticationStatus>("status");
        QTest::newRow("Invalid") << MiniCloud::Protocol::AuthenticationStatus::Invalid;
        QTest::newRow("Unknown") << static_cast<MiniCloud::Protocol::AuthenticationStatus>(999);
    }

    void authenticateResponse_invalidStatuses_serializeFails()
    {
        QFETCH(MiniCloud::Protocol::AuthenticationStatus, status);

        MiniCloud::Protocol::AuthenticateResponseData originalData;
        originalData.status = status;

        const MiniCloud::Protocol::AuthenticationEncodeResult encodeResult = MiniCloud::Protocol::serializeAuthenticateResponse(originalData);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::AuthenticationEncodeResult::Status::Failed);
        QVERIFY(encodeResult.payload.isEmpty());
        QVERIFY(!encodeResult.errorMessage.isEmpty());
    }

    void authenticateResponse_invalidWireStatuses_deserializeFails_data()
    {
        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("invalid-sentinel") << QByteArrayLiteral(R"({"status":0})");

        QTest::newRow("unknown") << QByteArrayLiteral(R"({"status":999})");

        QTest::newRow("fractional") << QByteArrayLiteral(R"({"status":1.5})");
    }

    void authenticateResponse_invalidWireStatuses_deserializeFails()
    {
        QFETCH(QByteArray, payload);

        const MiniCloud::Protocol::AuthenticateResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeAuthenticateResponse(payload);
        QCOMPARE(decodeResult.status, MiniCloud::Protocol::AuthenticateResponseDecodeResult::Status::Failed);
        QVERIFY(decodeResult.data.status == MiniCloud::Protocol::AuthenticationStatus::Invalid);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
    }

    void authenticateRequest_emptyRequiredField_serializeFails_data()
    {
        QTest::addColumn<QString>("productKey");
        QTest::addColumn<QString>("deviceId");
        QTest::newRow("empty-productKey") << QString() << QStringLiteral("device-id-456");
        QTest::newRow("empty-deviceId") << QStringLiteral("product-key-123") << QString();
    }

    void authenticateRequest_emptyRequiredField_serializeFails()
    {
        QFETCH(QString, productKey);
        QFETCH(QString, deviceId);

        MiniCloud::Protocol::AuthenticateRequestData originalData;
        originalData.productKey = productKey;
        originalData.deviceId = deviceId;

        const MiniCloud::Protocol::AuthenticationEncodeResult encodeResult = MiniCloud::Protocol::serializeAuthenticateRequest(originalData);
        QCOMPARE(encodeResult.status, MiniCloud::Protocol::AuthenticationEncodeResult::Status::Failed);
        QVERIFY(encodeResult.payload.isEmpty());
        QVERIFY(!encodeResult.errorMessage.isEmpty());
    }

    void authenticateRequest_invalidPayload_deserializeFails_data()
    {
        QTest::addColumn<QByteArray>("payload");
        QTest::newRow("malformed-json") << QByteArrayLiteral(R"({"productKey": "product-key-123", "deviceId": )");
        QTest::newRow("json-array") << QByteArrayLiteral(R"(["product-key-123", "device-id-456"])");
        QTest::newRow("missing-productKey") << QByteArrayLiteral(R"({"deviceId": "device-id-456"})");
        QTest::newRow("product-key-wrong-type") << QByteArrayLiteral(R"({"productKey": 123, "deviceId": "device-id-456"})");
        QTest::newRow("empty-product-key") << QByteArrayLiteral(R"({"productKey": "", "deviceId": "device-id-456"})");
        QTest::newRow("missing-device-id") << QByteArrayLiteral(R"({"productKey": "product-key-123"})");
        QTest::newRow("device-id-wrong-type") << QByteArrayLiteral(R"({"productKey": "product-key-123", "deviceId": 456})");
        QTest::newRow("empty-device-id") << QByteArrayLiteral(R"({"productKey": "product-key-123", "deviceId": ""})");
    }

    void authenticateRequest_invalidPayload_deserializeFails()
    {
        QFETCH(QByteArray, payload);

        const MiniCloud::Protocol::AuthenticateRequestDecodeResult decodeResult = MiniCloud::Protocol::deserializeAuthenticateRequest(payload);
        QCOMPARE(decodeResult.status, MiniCloud::Protocol::AuthenticateRequestDecodeResult::Status::Failed);
        QVERIFY(decodeResult.data.productKey.isEmpty());
        QVERIFY(decodeResult.data.deviceId.isEmpty());
        QVERIFY(!decodeResult.errorMessage.isEmpty());
    }

    void authenticateResponse_invalidPayload_deserializeFails_data()
    {
        QTest::addColumn<QByteArray>("payload");
        QTest::newRow("malformed-json") << QByteArrayLiteral(R"({"status": )");
        QTest::newRow("json-array") << QByteArrayLiteral(R"([1])");
        QTest::newRow("missing-status") << QByteArrayLiteral(R"({})");
        QTest::newRow("status-wrong-type") << QByteArrayLiteral(R"({"status": "1"})");
    }

    void authenticateResponse_invalidPayload_deserializeFails()
    {
        QFETCH(QByteArray, payload);

        const MiniCloud::Protocol::AuthenticateResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeAuthenticateResponse(payload);
        QCOMPARE(decodeResult.status, MiniCloud::Protocol::AuthenticateResponseDecodeResult::Status::Failed);
        QVERIFY(decodeResult.data.status == MiniCloud::Protocol::AuthenticationStatus::Invalid);
        QVERIFY(!decodeResult.errorMessage.isEmpty());
    }

    void authenticateRequest_extraFields_areIgnored()
    {
        const QByteArray payload = QByteArrayLiteral(R"({"productKey":"product-key-123","deviceId":"device-456","futureField":true})");

        const MiniCloud::Protocol::AuthenticateRequestDecodeResult decodeResult =
            MiniCloud::Protocol::deserializeAuthenticateRequest(payload);

        QCOMPARE(
            decodeResult.status,
            MiniCloud::Protocol::AuthenticateRequestDecodeResult::Status::Success);

        QCOMPARE(decodeResult.data.productKey, QStringLiteral("product-key-123"));
        QCOMPARE(decodeResult.data.deviceId, QStringLiteral("device-456"));
        QCOMPARE(decodeResult.errorMessage, QString());
    }

    void authenticateResponse_extraFields_areIgnored()
    {
        const QByteArray payload = QByteArrayLiteral(R"({"status":1,"productKey":"product-key-123","futureField":true})");

        const MiniCloud::Protocol::AuthenticateResponseDecodeResult decodeResult = MiniCloud::Protocol::deserializeAuthenticateResponse(payload);

        QCOMPARE(decodeResult.status, MiniCloud::Protocol::AuthenticateResponseDecodeResult::Status::Success);

        QCOMPARE(decodeResult.errorMessage, QString());

        QCOMPARE(decodeResult.data.status, MiniCloud::Protocol::AuthenticationStatus::Valid);
    }
};

QTEST_MAIN(AuthenticationCodecTest)
#include "authenticationcodectest.moc"