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

    struct FileManagerBrowseResult
    {
        FileManagerOperationStatus status = FileManagerOperationStatus::Failed;
        QString errorMessage;
        QList<MiniCloud::Protocol::FileEntryData> entries;
    };

    struct FileManagerOperationResult
    {
        FileManagerOperationStatus status =
            FileManagerOperationStatus::Failed;
        QString errorMessage;
        QString path;
    };

    class FileManager
    {
    public:
        explicit FileManager(QString storageRoot);

        FileManagerBrowseResult browse(const QString &logicalPath) const;

        FileManagerOperationResult createDirectory(const QString &parentLogicalPath, const QString &name) const;

    private:
        QString m_storageRoot;
    };
}
