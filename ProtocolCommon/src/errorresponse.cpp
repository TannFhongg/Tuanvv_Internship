#include "errorresponse.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include <cmath>

namespace
{
    bool isValidErrorCode(MiniCloud::Protocol::ErrorCode errorCode)
    {
        using MiniCloud::Protocol::ErrorCode;

        switch (errorCode)
        {
        case ErrorCode::InvalidRequest:
        case ErrorCode::AuthenticationFailed:
        case ErrorCode::FileNotFound:
        case ErrorCode::InternalServerError:
        case ErrorCode::InvalidFrame:
        case ErrorCode::UnsupportedProtocolVersion:
        case ErrorCode::InvalidMessageType:
        case ErrorCode::PayloadTooLarge:
            return true;
        case ErrorCode::None:
            return false;
        }

        return false;
    }
}

MiniCloud::Protocol::ErrorResponseEncodeResult MiniCloud::Protocol::serializeErrorResponse(const ErrorResponseData &data)
{
    ErrorResponseEncodeResult result;

    if (!isValidErrorCode(data.errorCode))
    {
        result.errorMessage = QStringLiteral("Error response contains an invalid error code.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("errorCode"), static_cast<int>(data.errorCode));
    object.insert(QStringLiteral("message"), data.message);
    object.insert(QStringLiteral("details"), data.details);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = ErrorResponseEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::ErrorResponseDecodeResult MiniCloud::Protocol::deserializeErrorResponse(const QByteArray &payload)
{
    ErrorResponseDecodeResult result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Error response payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue errorCodeValue = object.value(QStringLiteral("errorCode"));
    const QJsonValue messageValue = object.value(QStringLiteral("message"));
    const QJsonValue detailsValue = object.value(QStringLiteral("details"));

    if (!errorCodeValue.isDouble() || !messageValue.isString() || !detailsValue.isObject())
    {
        result.errorMessage = QStringLiteral("Error response JSON has missing or invalid fields.");
        return result;
    }

    const double numericErrorCode = errorCodeValue.toDouble();

    if (!std::isfinite(numericErrorCode) || std::floor(numericErrorCode) != numericErrorCode || numericErrorCode < 0 || numericErrorCode > static_cast<quint16>(ErrorCode::PayloadTooLarge))
    {
        result.errorMessage = QStringLiteral("Error response JSON has an invalid error code.");
        return result;
    }

    const ErrorCode errorCode = static_cast<ErrorCode>(static_cast<quint16>(numericErrorCode));

    if (!isValidErrorCode(errorCode))
    {
        result.errorMessage = QStringLiteral("Error response JSON has an invalid error code.");
        return result;
    }

    result.data.errorCode = errorCode;
    result.data.message = messageValue.toString();
    result.data.details = detailsValue.toObject();
    result.status = ErrorResponseDecodeResult::Status::Success;
    return result;
}
