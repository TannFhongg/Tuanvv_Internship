#pragma once
#include <QtGlobal>
#include <QMetaType>

namespace MiniCloud::Protocol
{
    using RequestId = quint64;
    using TaskId = quint64;

    enum class MessageType : quint16
    {
        Invalid = 0,
        AuthenticateRequest = 1,
        AuthenticateResponse = 2,
        ErrorResponse = 3,
        FileChunk = 4,

        BrowseRequest = 5,
        BrowseResponse = 6,

        CreateDirectoryRequest = 7,
        FileOperationResponse = 8,

        SearchRequest = 9,
        SearchResponse = 10,

        RenameRequest = 11,
        MoveRequest = 12,
        DeleteRequest = 13,

        UploadStartRequest = 14,
        UploadReadyResponse = 15,
        DownloadRequest = 16,
        DownloadStartResponse = 17
    };

    enum class ErrorCode : quint16
    {
        None = 0,
        InvalidRequest = 1,
        AuthenticationFailed = 2,
        FileNotFound = 3,
        InternalServerError = 4,
        InvalidFrame = 5,
        UnsupportedProtocolVersion = 6,
        InvalidMessageType = 7,
        PayloadTooLarge = 8,
    };
}

Q_DECLARE_METATYPE(MiniCloud::Protocol::ErrorCode)
