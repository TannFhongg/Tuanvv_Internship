#pragma once

#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <QMetaType>
#include "protocoltypes.h"


namespace MiniCloud::Protocol
{
    struct ErrorResponseData
    {
        ErrorCode errorCode = ErrorCode::None;
        QString message;
        QJsonObject details;
    };

    struct ErrorResponseEncodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        QByteArray payload;
        QString errorMessage;
    };

    struct ErrorResponseDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        ErrorResponseData data;
        QString errorMessage;
    };

    ErrorResponseEncodeResult serializeErrorResponse(const ErrorResponseData &data);
    ErrorResponseDecodeResult deserializeErrorResponse(const QByteArray &payload);
}

Q_DECLARE_METATYPE(MiniCloud::Protocol::ErrorResponseData)