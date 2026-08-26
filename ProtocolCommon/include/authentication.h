#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace MiniCloud::Protocol
{
    enum class AuthenticationStatus : quint16
    {
        Invalid = 0,
        Valid = 1,
        InvalidKey = 2,
        Disabled = 3,
        DeviceMismatch = 4
    };

    struct AuthenticateRequestData
    {
        QString productKey;
        QString deviceId;
    };

    struct AuthenticateResponseData
    {
        AuthenticationStatus status = AuthenticationStatus::Invalid;
    };

    struct AuthenticationEncodeResult
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

    struct AuthenticateRequestDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        AuthenticateRequestData data;
        QString errorMessage;
    };

    struct AuthenticateResponseDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        AuthenticateResponseData data;
        QString errorMessage;
    };

    AuthenticationEncodeResult serializeAuthenticateRequest(const AuthenticateRequestData &data);
    AuthenticateRequestDecodeResult deserializeAuthenticateRequest(const QByteArray &payload);

    AuthenticationEncodeResult serializeAuthenticateResponse(const AuthenticateResponseData &data);
    AuthenticateResponseDecodeResult deserializeAuthenticateResponse(const QByteArray &payload);
}

Q_DECLARE_METATYPE(MiniCloud::Protocol::AuthenticationStatus)
Q_DECLARE_METATYPE(MiniCloud::Protocol::AuthenticateRequestData)
Q_DECLARE_METATYPE(MiniCloud::Protocol::AuthenticateResponseData)
