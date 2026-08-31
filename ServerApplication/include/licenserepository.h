#pragma once

#include <optional>

#include <QHash>
#include <QList>
#include <QString>

#include "licenserecord.h"

namespace MiniCloud::Server
{

    enum class LicenseRepositoryStatus
    {
        Success,
        Failed
    };

    struct LicenseRepositoryResult
    {
        LicenseRepositoryStatus status{LicenseRepositoryStatus::Failed};
        QString errorMessage;
    };

    class LicenseRepository
    {
    public:
        explicit LicenseRepository(QString filePath);

        LicenseRepositoryResult load();

        qsizetype count() const noexcept;

        std::optional<LicenseRecord> findByProductKey(const QString &productKey) const;

        QList<LicenseRecord> getAllRecords() const;

        LicenseRepositoryResult insert(const LicenseRecord &record);
        LicenseRepositoryResult update(const LicenseRecord &record);

    private:
        QString m_filePath;
        QHash<QString, LicenseRecord> m_records;

        LicenseRepositoryResult saveRecords(const QHash<QString, LicenseRecord> &records) const;
    };
} 
