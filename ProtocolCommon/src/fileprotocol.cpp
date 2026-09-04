#include "fileprotocol.h"

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
        if (entry.name.isEmpty() || entry.path.isEmpty())
        {
            result.errorMessage = QStringLiteral("Browse response entries require non-empty names and paths.");
            return result;
        }

        if (!isValidFileEntryType(entry.type))
        {
            result.errorMessage = QStringLiteral("Browse response contains an invalid file entry type.");
            return result;
        }

        QJsonObject object;
        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("type"), static_cast<quint16>(entry.type));
        object.insert(QStringLiteral("sizeBytes"), QString::number(entry.sizeBytes));
        object.insert(QStringLiteral("lastModifiedUtcMs"), QString::number(entry.lastModifiedUtcMs));
        entries.append(object);
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
        if (!entryValue.isObject())
        {
            result.errorMessage = QStringLiteral("Browse response JSON contains an invalid entry.");
            return result;
        }

        const QJsonObject entryObject = entryValue.toObject();
        const QJsonValue nameValue = entryObject.value(QStringLiteral("name"));
        const QJsonValue entryPathValue = entryObject.value(QStringLiteral("path"));
        const QJsonValue typeValue = entryObject.value(QStringLiteral("type"));
        const QJsonValue sizeBytesValue = entryObject.value(QStringLiteral("sizeBytes"));
        const QJsonValue lastModifiedUtcMsValue = entryObject.value(QStringLiteral("lastModifiedUtcMs"));

        if (!nameValue.isString() || nameValue.toString().isEmpty() || !entryPathValue.isString() || entryPathValue.toString().isEmpty() || !typeValue.isDouble())
        {
            result.errorMessage = QStringLiteral("Browse response JSON contains an entry with missing or invalid fields.");
            return result;
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
            result.errorMessage = QStringLiteral("Browse response JSON contains an invalid file entry type.");
            return result;
        }

        quint64 sizeBytes = 0;
        quint64 lastModifiedUtcMs = 0;

        if (!parseUnsignedDecimal(sizeBytesValue, sizeBytes) || !parseUnsignedDecimal(lastModifiedUtcMsValue, lastModifiedUtcMs))
        {
            result.errorMessage = QStringLiteral("Browse response JSON contains invalid unsigned integer metadata.");
            return result;
        }

        FileEntryData entry;
        entry.name = nameValue.toString();
        entry.path = entryPathValue.toString();
        entry.type = type;
        entry.sizeBytes = sizeBytes;
        entry.lastModifiedUtcMs = lastModifiedUtcMs;

        candidate.entries.append(entry);
    }

    result.data = candidate;
    result.status = BrowseResponseDecodeResult::Status::Success;
    return result;
}
