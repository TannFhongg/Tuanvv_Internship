#pragma once

#include <QMetaType>
#include <protocoltypes.h>
namespace MiniCloud::Client
{
    enum class RequestDestination
    {
        Invalid,
        License,
        File,
        Task
    };

    enum class RequestSendStatus
    {
        Accepted,
        Failed
    };

    enum class RequestDispatchError
    {
        None,
        Unknown,
        InvalidRequest,
        InvalidDestination,
        NotConnected,
        InvalidMessageType,
        PayloadTooLarge,
        FrameEncodingFailed,
        SocketWriteFailed,
        ConnectionLost,
        RequestTimeout,
        ResponseTypeMismatch,
        InvalidResponsePayload,
        UnexpectedResponse
    };

    struct RequestSendResult
    {
        RequestSendStatus status = RequestSendStatus::Failed;
        MiniCloud::Protocol::RequestId requestId = 0;
        RequestDispatchError errorCode = RequestDispatchError::Unknown;
    };
}

Q_DECLARE_METATYPE(MiniCloud::Client::RequestDestination)
Q_DECLARE_METATYPE(MiniCloud::Client::RequestDispatchError)
