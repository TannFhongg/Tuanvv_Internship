#pragma once

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QSaveFile>
#include <QString>
#include <QStringList>

#include <memory>

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

    enum class FileManagerDownloadFailureReason
    {
        None,
        InvalidPath,
        NotFound,
        IoFailure,
        TransferInProgress
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

    struct FileManagerUploadStartResult
    {
        FileManagerOperationStatus status = FileManagerOperationStatus::Failed;
        QString errorMessage;
        bool completed = false;
        QString path;
    };

    struct FileManagerUploadChunkResult
    {
        FileManagerOperationStatus status = FileManagerOperationStatus::Failed;
        QString errorMessage;
        bool completed = false;
        QString path;
    };

    struct FileManagerDownloadStartResult
    {
        FileManagerOperationStatus status = FileManagerOperationStatus::Failed;
        FileManagerDownloadFailureReason failureReason = FileManagerDownloadFailureReason::None;
        QString errorMessage;
        QString path;
        quint64 totalSizeBytes = 0;
        bool completed = false;
    };

    struct FileManagerDownloadChunkResult
    {
        FileManagerOperationStatus status = FileManagerOperationStatus::Failed;
        QString errorMessage;
        quint64 offset = 0;
        QByteArray data;
        bool completed = false;
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

        FileManagerUploadStartResult beginUpload(const QString &destinationDirectoryPath, const QString &fileName, quint64 totalSizeBytes);

        FileManagerUploadChunkResult appendUploadChunk(quint64 offset, const QByteArray &bytes);

        void cancelUpload();

        FileManagerDownloadStartResult beginDownload(const QString &logicalPath);

        FileManagerDownloadChunkResult readNextDownloadChunk();

    private:
        void cancelActiveUpload();

        void cancelActiveDownload();

        QString m_storageRoot;

        std::unique_ptr<QSaveFile> m_activeUploadFile;
        QString m_activeUploadLogicalPath;
        QStringList m_activeUploadTemporaryFilesystemPaths;
        quint64 m_activeUploadTotalSizeBytes = 0;
        quint64 m_activeUploadReceivedBytes = 0;

        std::unique_ptr<QFile> m_activeDownloadFile;
        QString m_activeDownloadLogicalPath;
        quint64 m_activeDownloadTotalSizeBytes = 0;
        quint64 m_activeDownloadReadBytes = 0;
    };
}
