#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include "licenserepository.h"

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
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        result.errorMessage = QStringLiteral("Failed to open file for reading: %1").arg(m_filePath);
        return result;
    }
    QJsonParseError parseError;

    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
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
        const QJsonValue  productKeyValue = licenseObject.value(QStringLiteral("productKey"));
        const QJsonValue  deviceIdValue = licenseObject.value(QStringLiteral("deviceId"));
        const QJsonValue  enabledValue = licenseObject.value(QStringLiteral("enabled"));

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

        if (productKey.isEmpty())
        {
            result.errorMessage = QStringLiteral("Missing or invalid 'productKey' in file: %1").arg(m_filePath);
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
