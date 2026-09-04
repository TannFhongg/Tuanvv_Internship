#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace MiniCloud::Protocol
{
    inline constexpr qsizetype protocolMaxLogicalPathUtf8Bytes = 1024;
    inline constexpr qsizetype protocolMaxFileNameUtf8Bytes = 255;
    inline constexpr qsizetype protocolMaxSearchQueryUtf8Bytes = 255;

    struct BrowseRequestData
    {
        QString path;
    };

    struct FileProtocolEncodeResult
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

    struct BrowseRequestDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        BrowseRequestData data;
        QString errorMessage;
    };

    struct CreateDirectoryRequestData
    {
        QString parentPath;
        QString name;
    };

    struct FileOperationResponseData
    {
        QString path;
    };

    struct CreateDirectoryRequestDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        CreateDirectoryRequestData data;
        QString errorMessage;
    };

    struct FileOperationResponseDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        FileOperationResponseData data;
        QString errorMessage;
    };

    enum class FileEntryType : quint16
    {
        Invalid = 0,
        File = 1,
        Directory = 2
    };

    struct FileEntryData
    {
        QString name;
        QString path;
        FileEntryType type = FileEntryType::Invalid;
        quint64 sizeBytes = 0;
        quint64 lastModifiedUtcMs = 0;
    };

    struct BrowseResponseData
    {
        QString path;
        QList<FileEntryData> entries;
    };

    struct BrowseResponseDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        BrowseResponseData data;
        QString errorMessage;
    };

    struct SearchRequestData
    {
        QString path;
        QString query;
    };

    struct SearchResponseData
    {
        QString path;
        QList<FileEntryData> entries;
    };

    struct SearchRequestDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        SearchRequestData data;
        QString errorMessage;
    };

    struct SearchResponseDecodeResult
    {
        enum class Status
        {
            Success,
            Failed
        };

        Status status = Status::Failed;
        SearchResponseData data;
        QString errorMessage;
    };

    FileProtocolEncodeResult serializeBrowseRequest(const BrowseRequestData &data);

    BrowseRequestDecodeResult deserializeBrowseRequest(const QByteArray &payload);

    FileProtocolEncodeResult serializeCreateDirectoryRequest(const CreateDirectoryRequestData &data);

    CreateDirectoryRequestDecodeResult deserializeCreateDirectoryRequest(const QByteArray &payload);

    FileProtocolEncodeResult serializeFileOperationResponse(const FileOperationResponseData &data);

    FileOperationResponseDecodeResult deserializeFileOperationResponse(const QByteArray &payload);

    FileProtocolEncodeResult serializeBrowseResponse(const BrowseResponseData &data);

    BrowseResponseDecodeResult deserializeBrowseResponse(const QByteArray &payload);

    FileProtocolEncodeResult serializeSearchRequest(const SearchRequestData &data);

    SearchRequestDecodeResult deserializeSearchRequest(const QByteArray &payload);

    FileProtocolEncodeResult serializeSearchResponse(const SearchResponseData &data);

    SearchResponseDecodeResult deserializeSearchResponse(const QByteArray &payload);
}

Q_DECLARE_METATYPE(MiniCloud::Protocol::FileEntryType)
Q_DECLARE_METATYPE(MiniCloud::Protocol::FileEntryData)
Q_DECLARE_METATYPE(MiniCloud::Protocol::BrowseResponseData)
