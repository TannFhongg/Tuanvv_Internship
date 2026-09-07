#pragma once

#include <QList>
#include <QString>

#include "fileprotocol.h"

namespace MiniCloud::Server
{
    enum class FileManagerOperationStatus
    {
        Success,
        Failed
    };

    enum class FileManagerBrowseFailureReason
    {
        None,
        InvalidPath,
        NotFound,
        IoFailure
    };

    struct FileManagerBrowseResult
    {
        FileManagerOperationStatus status = FileManagerOperationStatus::Failed;
        FileManagerBrowseFailureReason failureReason = FileManagerBrowseFailureReason::None;
        QString errorMessage;
        QList<MiniCloud::Protocol::FileEntryData> entries;
    };

    struct FileManagerOperationResult
    {
        FileManagerOperationStatus status =FileManagerOperationStatus::Failed;
        QString errorMessage;
        QString path;
    };

    class FileManager
    {
    public:
        explicit FileManager(QString storageRoot);

        FileManagerBrowseResult browse(const QString &logicalPath) const;

        FileManagerBrowseResult search(const QString &directoryLogicalPath, const QString &query) const;

        FileManagerOperationResult createDirectory(const QString &parentLogicalPath, const QString &name) const;

        FileManagerOperationResult rename(const QString &sourceLogicalPath, const QString &newName) const;

        FileManagerOperationResult move(const QString &sourceLogicalPath, const QString &destinationDirectoryLogicalPath) const;

        FileManagerOperationResult remove(const QString &logicalPath) const;

    private:
        QString m_storageRoot;
    };
}
