#pragma once
#include <QString>
#include <QHash>
#include <optional>
#include "licenserecord.h"

using MiniCloud::Server::LicenseRecord;

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

private:
    QString m_filePath;
    QHash<QString, LicenseRecord> m_records;
};