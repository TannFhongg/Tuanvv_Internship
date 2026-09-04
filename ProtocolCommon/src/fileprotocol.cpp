#include "fileprotocol.h"
#include "protocolconstants.h"

#include <QDataStream>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace
{
    bool isNonBlankString(const QString &value)
    {
        return !value.trimmed().isEmpty();
    }

    bool isValidLogicalPathField(const QString &path)
    {
        return isNonBlankString(path) && path.toUtf8().size() <= MiniCloud::Protocol::protocolMaxLogicalPathUtf8Bytes;
    }

    bool isValidFileNameField(const QString &name)
    {
        return isNonBlankString(name) && name.toUtf8().size() <= MiniCloud::Protocol::protocolMaxFileNameUtf8Bytes;
    }

    bool isValidSearchQueryField(const QString &query)
    {
        return isNonBlankString(query) && query.toUtf8().size() <= MiniCloud::Protocol::protocolMaxSearchQueryUtf8Bytes;
    }

    bool isValidFileEntryType(MiniCloud::Protocol::FileEntryType type)
    {
        using MiniCloud::Protocol::FileEntryType;
        return type == FileEntryType::File || type == FileEntryType::Directory;
    }

    bool parseUnsignedDecimal(const QJsonValue &value, quint64 &output)
    {
        if (!value.isString())
        {
            return false;
        }

        const QString text = value.toString();

        static const QRegularExpression pattern(QStringLiteral("^(0|[1-9][0-9]*)$"));

        if (!pattern.match(text).hasMatch())
        {
            return false;
        }

        bool ok = false;
        const quint64 parsed = text.toULongLong(&ok);

        if (!ok)
        {
            return false;
        }

        output = parsed;
        return true;
    }

    bool tryEncodeFileEntry(const MiniCloud::Protocol::FileEntryData &entry, QJsonObject &object)
    {
        if (entry.name.isEmpty() || entry.path.isEmpty() || !isValidFileEntryType(entry.type))
        {
            return false;
        }

        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("type"), static_cast<quint16>(entry.type));
        object.insert(QStringLiteral("sizeBytes"), QString::number(entry.sizeBytes));
        object.insert(QStringLiteral("lastModifiedUtcMs"), QString::number(entry.lastModifiedUtcMs));
        return true;
    }

    bool tryDecodeFileEntry(const QJsonValue &entryValue, MiniCloud::Protocol::FileEntryData &entry)
    {
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;

        if (!entryValue.isObject())
        {
            return false;
        }

        const QJsonObject object = entryValue.toObject();
        const QJsonValue nameValue = object.value(QStringLiteral("name"));
        const QJsonValue pathValue = object.value(QStringLiteral("path"));
        const QJsonValue typeValue = object.value(QStringLiteral("type"));
        const QJsonValue sizeBytesValue = object.value(QStringLiteral("sizeBytes"));
        const QJsonValue lastModifiedUtcMsValue = object.value(QStringLiteral("lastModifiedUtcMs"));

        if (!nameValue.isString() || nameValue.toString().isEmpty() || !pathValue.isString() || pathValue.toString().isEmpty() || !typeValue.isDouble())
        {
            return false;
        }

        const double rawType = typeValue.toDouble();
        FileEntryType type = FileEntryType::Invalid;

        if (rawType == static_cast<double>(static_cast<quint16>(FileEntryType::File)))
        {
            type = FileEntryType::File;
        }
        else if (rawType == static_cast<double>(static_cast<quint16>(FileEntryType::Directory)))
        {
            type = FileEntryType::Directory;
        }
        else
        {
            return false;
        }

        quint64 sizeBytes = 0;
        quint64 lastModifiedUtcMs = 0;

        if (!parseUnsignedDecimal(sizeBytesValue, sizeBytes) || !parseUnsignedDecimal(lastModifiedUtcMsValue, lastModifiedUtcMs))
        {
            return false;
        }

        FileEntryData candidate;
        candidate.name = nameValue.toString();
        candidate.path = pathValue.toString();
        candidate.type = type;
        candidate.sizeBytes = sizeBytes;
        candidate.lastModifiedUtcMs = lastModifiedUtcMs;

        entry = candidate;
        return true;
    }
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeBrowseRequest(const BrowseRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("Browse request requires a non-blank path within the UTF-8 byte limit.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::BrowseRequestDecodeResult MiniCloud::Protocol::deserializeBrowseRequest(const QByteArray &payload)
{
    BrowseRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Browse request payload must be a JSON object.");
        return result;
    }

    const QJsonValue pathValue = document.object().value(QStringLiteral("path"));

    if (!pathValue.isString())
    {
        result.errorMessage = QStringLiteral("Browse request JSON has a missing or invalid path.");
        return result;
    }

    const QString path = pathValue.toString();

    if (!isValidLogicalPathField(path))
    {
        result.errorMessage = QStringLiteral("Browse request JSON has a missing or invalid path.");
        return result;
    }

    result.data.path = path;
    result.status = BrowseRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeCreateDirectoryRequest(const CreateDirectoryRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.parentPath) || !isValidFileNameField(data.name))
    {
        result.errorMessage = QStringLiteral("Create directory request has invalid parent path or name.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("parentPath"), data.parentPath);
    object.insert(QStringLiteral("name"), data.name);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::CreateDirectoryRequestDecodeResult MiniCloud::Protocol::deserializeCreateDirectoryRequest(const QByteArray &payload)
{
    CreateDirectoryRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Create directory request payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue parentPathValue = object.value(QStringLiteral("parentPath"));
    const QJsonValue nameValue = object.value(QStringLiteral("name"));

    if (!parentPathValue.isString() || !nameValue.isString())
    {
        result.errorMessage = QStringLiteral("Create directory request JSON has missing or invalid fields.");
        return result;
    }

    const QString parentPath = parentPathValue.toString();
    const QString name = nameValue.toString();

    if (!isValidLogicalPathField(parentPath) || !isValidFileNameField(name))
    {
        result.errorMessage = QStringLiteral("Create directory request JSON has missing or invalid fields.");
        return result;
    }

    CreateDirectoryRequestData candidate;
    candidate.parentPath = parentPath;
    candidate.name = name;

    result.data = candidate;
    result.status = CreateDirectoryRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeFileOperationResponse(const FileOperationResponseData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("File operation response requires a valid logical path.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileOperationResponseDecodeResult MiniCloud::Protocol::deserializeFileOperationResponse(const QByteArray &payload)
{
    FileOperationResponseDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("File operation response payload must be a JSON object.");
        return result;
    }

    const QJsonValue pathValue = document.object().value(QStringLiteral("path"));

    if (!pathValue.isString())
    {
        result.errorMessage = QStringLiteral("File operation response JSON has a missing or invalid path.");
        return result;
    }

    const QString candidatePath = pathValue.toString();

    if (!isValidLogicalPathField(candidatePath))
    {
        result.errorMessage = QStringLiteral("File operation response JSON has a missing or invalid path.");
        return result;
    }

    result.data.path = candidatePath;
    result.status = FileOperationResponseDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeBrowseResponse(const BrowseResponseData &data)
{
    FileProtocolEncodeResult result;

    if (data.path.isEmpty())
    {
        result.errorMessage = QStringLiteral("Browse response requires a non-empty path.");
        return result;
    }

    QJsonArray entries;

    for (const FileEntryData &entry : data.entries)
    {
        QJsonObject entryObject;

        if (!tryEncodeFileEntry(entry, entryObject))
        {
            result.errorMessage = QStringLiteral("Browse response contains an invalid file entry.");
            return result;
        }

        entries.append(entryObject);
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);
    object.insert(QStringLiteral("entries"), entries);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::BrowseResponseDecodeResult MiniCloud::Protocol::deserializeBrowseResponse(const QByteArray &payload)
{
    BrowseResponseDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Browse response payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue pathValue = object.value(QStringLiteral("path"));
    const QJsonValue entriesValue = object.value(QStringLiteral("entries"));

    if (!pathValue.isString() || pathValue.toString().isEmpty())
    {
        result.errorMessage = QStringLiteral("Browse response JSON has a missing or invalid path.");
        return result;
    }

    if (!entriesValue.isArray())
    {
        result.errorMessage = QStringLiteral("Browse response JSON has missing or invalid entries.");
        return result;
    }

    BrowseResponseData candidate;
    candidate.path = pathValue.toString();

    for (const QJsonValue &entryValue : entriesValue.toArray())
    {
        FileEntryData entry;

        if (!tryDecodeFileEntry(entryValue, entry))
        {
            result.errorMessage = QStringLiteral("Browse response JSON contains an invalid entry.");
            return result;
        }

        candidate.entries.append(entry);
    }

    result.data = candidate;
    result.status = BrowseResponseDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeSearchRequest(const SearchRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path) || !isValidSearchQueryField(data.query))
    {
        result.errorMessage = QStringLiteral("Search request has invalid path or query.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);
    object.insert(QStringLiteral("query"), data.query);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::SearchRequestDecodeResult MiniCloud::Protocol::deserializeSearchRequest(const QByteArray &payload)
{
    SearchRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Search request payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue pathValue = object.value(QStringLiteral("path"));
    const QJsonValue queryValue = object.value(QStringLiteral("query"));

    if (!pathValue.isString() || !queryValue.isString())
    {
        result.errorMessage = QStringLiteral("Search request JSON has missing or invalid fields.");
        return result;
    }

    const QString path = pathValue.toString();
    const QString query = queryValue.toString();

    if (!isValidLogicalPathField(path) || !isValidSearchQueryField(query))
    {
        result.errorMessage = QStringLiteral("Search request JSON has missing or invalid fields.");
        return result;
    }

    SearchRequestData candidate;
    candidate.path = path;
    candidate.query = query;

    result.data = candidate;
    result.status = SearchRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeSearchResponse(const SearchResponseData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("Search response has an invalid path.");
        return result;
    }

    QJsonArray entries;

    for (const FileEntryData &entry : data.entries)
    {
        QJsonObject entryObject;

        if (!tryEncodeFileEntry(entry, entryObject))
        {
            result.errorMessage = QStringLiteral("Search response contains an invalid file entry.");
            return result;
        }

        entries.append(entryObject);
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);
    object.insert(QStringLiteral("entries"), entries);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::SearchResponseDecodeResult MiniCloud::Protocol::deserializeSearchResponse(const QByteArray &payload)
{
    SearchResponseDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Search response payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue pathValue = object.value(QStringLiteral("path"));
    const QJsonValue entriesValue = object.value(QStringLiteral("entries"));

    if (!pathValue.isString() || !isValidLogicalPathField(pathValue.toString()))
    {
        result.errorMessage = QStringLiteral("Search response JSON has a missing or invalid path.");
        return result;
    }

    if (!entriesValue.isArray())
    {
        result.errorMessage = QStringLiteral("Search response JSON has missing or invalid entries.");
        return result;
    }

    SearchResponseData candidate;
    candidate.path = pathValue.toString();

    for (const QJsonValue &entryValue : entriesValue.toArray())
    {
        FileEntryData entry;

        if (!tryDecodeFileEntry(entryValue, entry))
        {
            result.errorMessage = QStringLiteral("Search response JSON contains an invalid entry.");
            return result;
        }

        candidate.entries.append(entry);
    }

    result.data = candidate;
    result.status = SearchResponseDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeRenameRequest(const RenameRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path) || !isValidFileNameField(data.newName))
    {
        result.errorMessage = QStringLiteral("Rename request has invalid path or new name.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);
    object.insert(QStringLiteral("newName"), data.newName);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::RenameRequestDecodeResult MiniCloud::Protocol::deserializeRenameRequest(const QByteArray &payload)
{
    RenameRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Rename request payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue pathValue = object.value(QStringLiteral("path"));
    const QJsonValue newNameValue = object.value(QStringLiteral("newName"));

    if (!pathValue.isString() || !newNameValue.isString())
    {
        result.errorMessage = QStringLiteral("Rename request JSON has missing or invalid fields.");
        return result;
    }

    const QString path = pathValue.toString();
    const QString newName = newNameValue.toString();

    if (!isValidLogicalPathField(path) || !isValidFileNameField(newName))
    {
        result.errorMessage = QStringLiteral("Rename request JSON has missing or invalid fields.");
        return result;
    }

    RenameRequestData candidate;
    candidate.path = path;
    candidate.newName = newName;

    result.data = candidate;
    result.status = RenameRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeMoveRequest(const MoveRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.sourcePath) || !isValidLogicalPathField(data.destinationDirectoryPath))
    {
        result.errorMessage = QStringLiteral("Move request has invalid source or destination path.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("sourcePath"), data.sourcePath);
    object.insert(QStringLiteral("destinationDirectoryPath"), data.destinationDirectoryPath);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::MoveRequestDecodeResult MiniCloud::Protocol::deserializeMoveRequest(const QByteArray &payload)
{
    MoveRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Move request payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue sourcePathValue = object.value(QStringLiteral("sourcePath"));
    const QJsonValue destinationDirectoryPathValue = object.value(QStringLiteral("destinationDirectoryPath"));

    if (!sourcePathValue.isString() || !destinationDirectoryPathValue.isString())
    {
        result.errorMessage = QStringLiteral("Move request JSON has missing or invalid fields.");
        return result;
    }

    const QString sourcePath = sourcePathValue.toString();
    const QString destinationDirectoryPath = destinationDirectoryPathValue.toString();

    if (!isValidLogicalPathField(sourcePath) || !isValidLogicalPathField(destinationDirectoryPath))
    {
        result.errorMessage = QStringLiteral("Move request JSON has missing or invalid fields.");
        return result;
    }

    MoveRequestData candidate;
    candidate.sourcePath = sourcePath;
    candidate.destinationDirectoryPath = destinationDirectoryPath;

    result.data = candidate;
    result.status = MoveRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeDeleteRequest(const DeleteRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("Delete request has an invalid path.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::DeleteRequestDecodeResult MiniCloud::Protocol::deserializeDeleteRequest(const QByteArray &payload)
{
    DeleteRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Delete request payload must be a JSON object.");
        return result;
    }

    const QJsonValue pathValue = document.object().value(QStringLiteral("path"));

    if (!pathValue.isString())
    {
        result.errorMessage = QStringLiteral("Delete request JSON has a missing or invalid path.");
        return result;
    }

    const QString path = pathValue.toString();

    if (!isValidLogicalPathField(path))
    {
        result.errorMessage = QStringLiteral("Delete request JSON has a missing or invalid path.");
        return result;
    }

    DeleteRequestData candidate;
    candidate.path = path;

    result.data = candidate;
    result.status = DeleteRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeUploadStartRequest(const UploadStartRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.destinationDirectoryPath) || !isValidFileNameField(data.fileName))
    {
        result.errorMessage = QStringLiteral("Upload start request has an invalid destination path or file name.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("destinationDirectoryPath"), data.destinationDirectoryPath);
    object.insert(QStringLiteral("fileName"), data.fileName);
    object.insert(QStringLiteral("totalSizeBytes"), QString::number(data.totalSizeBytes));

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::UploadStartRequestDecodeResult MiniCloud::Protocol::deserializeUploadStartRequest(const QByteArray &payload)
{
    UploadStartRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Upload start request payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue destinationDirectoryPathValue = object.value(QStringLiteral("destinationDirectoryPath"));
    const QJsonValue fileNameValue = object.value(QStringLiteral("fileName"));
    const QJsonValue totalSizeBytesValue = object.value(QStringLiteral("totalSizeBytes"));

    if (!destinationDirectoryPathValue.isString() || !fileNameValue.isString())
    {
        result.errorMessage = QStringLiteral("Upload start request JSON has missing or invalid fields.");
        return result;
    }

    const QString destinationDirectoryPath = destinationDirectoryPathValue.toString();
    const QString fileName = fileNameValue.toString();
    quint64 totalSizeBytes = 0;

    if (!isValidLogicalPathField(destinationDirectoryPath) || !isValidFileNameField(fileName) || !parseUnsignedDecimal(totalSizeBytesValue, totalSizeBytes))
    {
        result.errorMessage = QStringLiteral("Upload start request JSON has missing or invalid fields.");
        return result;
    }

    UploadStartRequestData candidate;
    candidate.destinationDirectoryPath = destinationDirectoryPath;
    candidate.fileName = fileName;
    candidate.totalSizeBytes = totalSizeBytes;

    result.data = candidate;
    result.status = UploadStartRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeUploadReadyResponse(const UploadReadyResponseData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("Upload ready response has an invalid path.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::UploadReadyResponseDecodeResult MiniCloud::Protocol::deserializeUploadReadyResponse(const QByteArray &payload)
{
    UploadReadyResponseDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Upload ready response payload must be a JSON object.");
        return result;
    }

    const QJsonValue pathValue = document.object().value(QStringLiteral("path"));

    if (!pathValue.isString() || !isValidLogicalPathField(pathValue.toString()))
    {
        result.errorMessage = QStringLiteral("Upload ready response JSON has a missing or invalid path.");
        return result;
    }

    UploadReadyResponseData candidate;
    candidate.path = pathValue.toString();

    result.data = candidate;
    result.status = UploadReadyResponseDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeDownloadRequest(const DownloadRequestData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("Download request has an invalid path.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::DownloadRequestDecodeResult MiniCloud::Protocol::deserializeDownloadRequest(const QByteArray &payload)
{
    DownloadRequestDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Download request payload must be a JSON object.");
        return result;
    }

    const QJsonValue pathValue = document.object().value(QStringLiteral("path"));

    if (!pathValue.isString() || !isValidLogicalPathField(pathValue.toString()))
    {
        result.errorMessage = QStringLiteral("Download request JSON has a missing or invalid path.");
        return result;
    }

    DownloadRequestData candidate;
    candidate.path = pathValue.toString();

    result.data = candidate;
    result.status = DownloadRequestDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeDownloadStartResponse(const DownloadStartResponseData &data)
{
    FileProtocolEncodeResult result;

    if (!isValidLogicalPathField(data.path))
    {
        result.errorMessage = QStringLiteral("Download start response has an invalid path.");
        return result;
    }

    QJsonObject object;
    object.insert(QStringLiteral("path"), data.path);
    object.insert(QStringLiteral("totalSizeBytes"), QString::number(data.totalSizeBytes));

    result.payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::DownloadStartResponseDecodeResult MiniCloud::Protocol::deserializeDownloadStartResponse(const QByteArray &payload)
{
    DownloadStartResponseDecodeResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = parseError.errorString();
        return result;
    }

    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("Download start response payload must be a JSON object.");
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue pathValue = object.value(QStringLiteral("path"));
    const QJsonValue totalSizeBytesValue = object.value(QStringLiteral("totalSizeBytes"));

    if (!pathValue.isString())
    {
        result.errorMessage = QStringLiteral("Download start response JSON has missing or invalid fields.");
        return result;
    }

    const QString path = pathValue.toString();
    quint64 totalSizeBytes = 0;

    if (!isValidLogicalPathField(path) || !parseUnsignedDecimal(totalSizeBytesValue, totalSizeBytes))
    {
        result.errorMessage = QStringLiteral("Download start response JSON has missing or invalid fields.");
        return result;
    }

    DownloadStartResponseData candidate;
    candidate.path = path;
    candidate.totalSizeBytes = totalSizeBytes;

    result.data = candidate;
    result.status = DownloadStartResponseDecodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileProtocolEncodeResult MiniCloud::Protocol::serializeFileChunk(const FileChunkData &data)
{
    FileProtocolEncodeResult result;

    if (data.bytes.isEmpty() || data.bytes.size() > static_cast<qsizetype>(protocolMaxFileChunkDataBytes))
    {
        result.errorMessage = QStringLiteral("File chunk data must be non-empty and within the maximum size.");
        return result;
    }

    QDataStream stream(&result.payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << data.offset;

    const int written = stream.writeRawData(data.bytes.constData(), static_cast<int>(data.bytes.size()));

    if (stream.status() != QDataStream::Ok || written != static_cast<int>(data.bytes.size()))
    {
        result.payload.clear();
        result.errorMessage = QStringLiteral("Failed to serialize file chunk data.");
        return result;
    }

    result.status = FileProtocolEncodeResult::Status::Success;
    return result;
}

MiniCloud::Protocol::FileChunkDecodeResult MiniCloud::Protocol::deserializeFileChunk(const QByteArray &payload)
{
    FileChunkDecodeResult result;

    const qsizetype offsetSize = static_cast<qsizetype>(sizeof(quint64));

    if (payload.size() <= offsetSize || payload.size() - offsetSize > static_cast<qsizetype>(protocolMaxFileChunkDataBytes))
    {
        result.errorMessage = QStringLiteral("File chunk payload has an invalid size.");
        return result;
    }

    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_0);

    quint64 offset = 0;
    stream >> offset;

    if (stream.status() != QDataStream::Ok)
    {
        result.errorMessage = QStringLiteral("Failed to deserialize file chunk offset.");
        return result;
    }

    FileChunkData candidate;
    candidate.offset = offset;
    candidate.bytes = payload.mid(offsetSize);

    result.data = candidate;
    result.status = FileChunkDecodeResult::Status::Success;
    return result;
}
