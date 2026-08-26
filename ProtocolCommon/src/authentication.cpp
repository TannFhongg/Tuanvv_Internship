#include "authentication.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace
{
    bool isValidAuthenticationStatus(MiniCloud::Protocol::AuthenticationStatus status)
    {
        using MiniCloud::Protocol::AuthenticationStatus;

        switch (status)
        {
        case AuthenticationStatus::Valid:
        case AuthenticationStatus::InvalidKey:
        case AuthenticationStatus::Disabled:
        case AuthenticationStatus::DeviceMismatch:
            return true;
        case AuthenticationStatus::Invalid:
        default:
            return false;
        }
    }

    bool isNonEmptyString(const QJsonValue &value)
    {
        return value.isString() && !value.toString().isEmpty();
    }
}

MiniCloud::Protocol::AuthenticationEncodeResult MiniCloud::Protocol::serializeAuthenticateRequest(const AuthenticateRequestData &data)
{
    AuthenticationEncodeResult result;

    if (data.productKey.isEmpty() || data.deviceId.isEmpty())
    {
        result.errorMessage = QStringLiteral("Authentication request requires a non-empty product key and device ID.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("productKey"), data.productKey);
    object.insert(QStringLiteral("deviceId"), data.deviceId);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = AuthenticationEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::AuthenticateRequestDecodeResult MiniCloud::Protocol::deserializeAuthenticateRequest(const QByteArray &payload)
{
    AuthenticateRequestDecodeResult result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Authentication request payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue productKeyValue = object.value(QStringLiteral("productKey"));
    const QJsonValue deviceIdValue = object.value(QStringLiteral("deviceId"));

    if (!isNonEmptyString(productKeyValue) || !isNonEmptyString(deviceIdValue))
    {
        result.errorMessage = QStringLiteral("Authentication request JSON has missing or invalid fields.");
        return result;
    }

    result.data.productKey = productKeyValue.toString();
    result.data.deviceId = deviceIdValue.toString();
    result.status = AuthenticateRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::AuthenticationEncodeResult MiniCloud::Protocol::serializeAuthenticateResponse(const AuthenticateResponseData &data)
{
    AuthenticationEncodeResult result;

    if (!isValidAuthenticationStatus(data.status))
    {
        result.errorMessage = QStringLiteral("Authentication response contains an invalid status.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("status"), static_cast<int>(data.status));

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = AuthenticationEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::AuthenticateResponseDecodeResult MiniCloud::Protocol::deserializeAuthenticateResponse(const QByteArray &payload)
{
    AuthenticateResponseDecodeResult result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Authentication response payload must be a JSON object.");
        return result;
    }

    const QJsonValue statusValue = document.object().value(QStringLiteral("status"));

    if (!statusValue.isDouble())
    {
        result.errorMessage = QStringLiteral("Authentication response JSON has a missing or invalid status.");
        return result;
    }

    const double numericStatus = statusValue.toDouble();

    if (!std::isfinite(numericStatus)
        || std::floor(numericStatus) != numericStatus
        || numericStatus < static_cast<double>(static_cast<quint16>(AuthenticationStatus::Valid))
        || numericStatus > static_cast<double>(static_cast<quint16>(AuthenticationStatus::DeviceMismatch)))
    {
        result.errorMessage = QStringLiteral("Authentication response JSON has an invalid status.");
        return result;
    }

    const AuthenticationStatus status =
        static_cast<AuthenticationStatus>(static_cast<quint16>(numericStatus));

    if (!isValidAuthenticationStatus(status))
    {
        result.errorMessage = QStringLiteral("Authentication response JSON has an invalid status.");
        return result;
    }

    result.data.status = status;
    result.status = AuthenticateResponseDecodeResult::Status::Success;
    return result;
}
