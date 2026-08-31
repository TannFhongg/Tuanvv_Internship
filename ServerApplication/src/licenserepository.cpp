#include "licenserepository.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <utility>

namespace MiniCloud::Server
{

LicenseRepository::LicenseRepository(QString filePath)
    : m_filePath(std::move(filePath))
{
}

LicenseRepositoryResult LicenseRepository::load()
{
    LicenseRepositoryResult result;

    if (!QFileInfo::exists(m_filePath))
    {
        m_records.clear();
        result.status = LicenseRepositoryStatus::Success;
        result.errorMessage.clear();
        return result;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        result.errorMessage = QStringLiteral("Failed to open file for reading: %1").arg(m_filePath);
        return result;
    }

    const QByteArray payload = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        result.errorMessage = QStringLiteral("Failed to read file: %1").arg(m_filePath);
        return result;
    }

    QJsonParseError parseError;

    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        result.errorMessage = QStringLiteral("Failed to parse JSON from file: %1").arg(m_filePath);
        return result;
    }
    if (!document.isObject())
    {
        result.errorMessage = QStringLiteral("JSON document is not an object in file: %1").arg(m_filePath);
        return result;
    }
    const QJsonObject jsonObject = document.object();
    const QJsonValue schemaVersionValue = jsonObject.value(QStringLiteral("schemaVersion"));

    if (!schemaVersionValue.isDouble())
    {
        result.errorMessage = QStringLiteral("Invalid or missing schemaVersion in file: %1").arg(m_filePath);
        return result;
    }
    const double schemaVersion = schemaVersionValue.toDouble();

    if (!std::isfinite(schemaVersion) || std::floor(schemaVersion) != schemaVersion || schemaVersion != 1.0)
    {
        result.errorMessage = QStringLiteral("Invalid schemaVersion in file: %1").arg(m_filePath);
        return result;
    }

    const QJsonValue licensesValue = jsonObject.value(QStringLiteral("licenses"));
    if (!licensesValue.isArray())
    {
        result.errorMessage = QStringLiteral("Missing or invalid 'licenses' array in file: %1").arg(m_filePath);
        return result;
    }
    const QJsonArray licensesArray = licensesValue.toArray();
    QHash<QString, LicenseRecord> tempRecords;

    for (const QJsonValue &licenseValue : licensesArray)
    {
        if (!licenseValue.isObject())
        {
            result.errorMessage = QStringLiteral("Invalid license entry in file: %1").arg(m_filePath);
            return result;
        }

        const QJsonObject licenseObject = licenseValue.toObject();
        const QJsonValue productKeyValue = licenseObject.value(QStringLiteral("productKey"));
        const QJsonValue deviceIdValue = licenseObject.value(QStringLiteral("deviceId"));
        const QJsonValue enabledValue = licenseObject.value(QStringLiteral("enabled"));

        if (!productKeyValue.isString() || productKeyValue.toString().isEmpty())
        {
            result.errorMessage = QStringLiteral("Missing or invalid productKey.");
            return result;
        }

        if (!deviceIdValue.isString())
        {
            result.errorMessage = QStringLiteral("Missing or invalid deviceId.");
            return result;
        }

        if (!enabledValue.isBool())
        {
            result.errorMessage = QStringLiteral("Missing or invalid enabled.");
            return result;
        }

        const QString productKey = productKeyValue.toString();

        if (tempRecords.contains(productKey))
        {
            result.errorMessage = QStringLiteral("Duplicate productKey: %1").arg(productKey);
            return result;
        }

        tempRecords.insert(productKey, LicenseRecord{productKey, deviceIdValue.toString(), enabledValue.toBool()});
    }

    m_records = std::move(tempRecords);

    result.status = LicenseRepositoryStatus::Success;
    result.errorMessage.clear();
    return result;
}

qsizetype LicenseRepository::count() const noexcept
{
    return m_records.size();
}

std::optional<LicenseRecord> LicenseRepository::findByProductKey(const QString &productKey) const
{
    const auto iterator = m_records.constFind(productKey);

    if (iterator == m_records.constEnd())
    {
        return std::nullopt;
    }
    return *iterator;
}

QList<LicenseRecord> LicenseRepository::getAllRecords() const
{
    QList<LicenseRecord> records = m_records.values();

    std::sort(
        records.begin(),
        records.end(),
        [](const LicenseRecord &left, const LicenseRecord &right)
        {
            return left.productKey < right.productKey;
        });

    return records;
}

LicenseRepositoryResult LicenseRepository::insert(const LicenseRecord &record)
{
    LicenseRepositoryResult result;
    if (record.productKey.isEmpty())
    {
        result.errorMessage = QStringLiteral("Product key cannot be empty.");
        return result;
    }

    if (m_records.contains(record.productKey))
    {
        result.errorMessage = QStringLiteral("Product key already exists: %1").arg(record.productKey);
        return result;
    }

    QHash<QString, LicenseRecord> candidate = m_records;
    candidate.insert(record.productKey, record);

    const LicenseRepositoryResult saveResult = saveRecords(candidate);

    if (saveResult.status == LicenseRepositoryStatus::Failed)
    {
        return saveResult;
    }

    m_records = std::move(candidate);

    result.status = LicenseRepositoryStatus::Success;
    return result;
}

LicenseRepositoryResult LicenseRepository::update(const LicenseRecord &record)
{
    LicenseRepositoryResult result;

    if (record.productKey.isEmpty())
    {
        result.errorMessage = QStringLiteral("Product key cannot be empty.");
        return result;
    }

    if (!m_records.contains(record.productKey))
    {
        result.errorMessage = QStringLiteral(
            "Product key does not exist: %1").arg(record.productKey);
        return result;
    }

    QHash<QString, LicenseRecord> candidate = m_records;
    candidate[record.productKey] = record;

    const LicenseRepositoryResult saveResult = saveRecords(candidate);

    if (saveResult.status == LicenseRepositoryStatus::Failed)
    {
        return saveResult;
    }

    m_records = std::move(candidate);

    result.status = LicenseRepositoryStatus::Success;
    return result;
}

LicenseRepositoryResult LicenseRepository::saveRecords(const QHash<QString, LicenseRecord> &records) const
{
    LicenseRepositoryResult result;
    QJsonArray licensesArray;

    for (const LicenseRecord &record : records)
    {
        QJsonObject recordObject;
        recordObject.insert(QStringLiteral("productKey"), record.productKey);
        recordObject.insert(QStringLiteral("deviceId"), record.deviceId);
        recordObject.insert(QStringLiteral("enabled"), record.enabled);

        licensesArray.append(recordObject);
    }
    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("schemaVersion"), 1);
    rootObject.insert(QStringLiteral("licenses"), licensesArray);

    QJsonDocument document(rootObject);
    const QByteArray payload = document.toJson(QJsonDocument::Compact);

    QSaveFile file(m_filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        result.errorMessage = QStringLiteral("Failed to open file for writing: %1").arg(m_filePath);
        return result;
    }

    const qint64 writtenBytes = file.write(payload);

    if (writtenBytes != static_cast<qint64>(payload.size()))
    {
        file.cancelWriting();
        result.errorMessage = QStringLiteral("Failed to write all data to file: %1").arg(m_filePath);
        return result;
    }

    if (!file.commit())
    {
        result.errorMessage = QStringLiteral("Failed to commit changes to file: %1").arg(m_filePath);
        return result;
    }

    result.status = LicenseRepositoryStatus::Success;
    return result;
}

} 
