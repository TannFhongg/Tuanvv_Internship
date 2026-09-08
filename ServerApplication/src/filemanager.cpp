#include "filemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTimeZone>

#include <algorithm>
#include <utility>

namespace
{
    constexpr qsizetype maximumTransferChunkSize = 64 * 1024;

    bool isCanonicalLogicalPath(const QString &logicalPath)
    {
        if (logicalPath == QStringLiteral("/"))
        {
            return true;
        }

        if (logicalPath.isEmpty() || !logicalPath.startsWith(QLatin1Char('/')) || logicalPath.endsWith(QLatin1Char('/')) || logicalPath.contains(QLatin1Char('\\')))
        {
            return false;
        }

        const QStringList segments = logicalPath.mid(1).split(QLatin1Char('/'), Qt::KeepEmptyParts);

        for (const QString &segment : segments)
        {
            if (segment.isEmpty() || segment == QStringLiteral(".") || segment == QStringLiteral(".."))
            {
                return false;
            }
        }

        return true;
    }

    bool isValidLeafName(const QString &name)
    {
        return !name.trimmed().isEmpty() && name != QStringLiteral(".") && name != QStringLiteral("..") && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'));
    }
}

namespace MiniCloud::Server
{
    FileManager::FileManager(QString storageRoot) : m_storageRoot(std::move(storageRoot)) {}

    FileManagerBrowseResult FileManager::browse(const QString &logicalPath) const
    {
        FileManagerBrowseResult result;

        if (!isCanonicalLogicalPath(logicalPath))
        {
            result.failureReason = FileManagerBrowseFailureReason::InvalidPath;
            result.errorMessage = QStringLiteral("Logical path must be canonical.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.failureReason = FileManagerBrowseFailureReason::IoFailure;
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString relativePath = logicalPath.mid(1);

        const QString filesystemPath = QDir(m_storageRoot).filePath(relativePath);
        const QFileInfo browseDirectoryInfo(filesystemPath);

        if (!browseDirectoryInfo.exists() || !browseDirectoryInfo.isDir())
        {
            result.failureReason = FileManagerBrowseFailureReason::NotFound;
            result.errorMessage = QStringLiteral("Logical path is not an existing directory.");
            return result;
        }

        const QDir storageDirectory(filesystemPath);

        if (!storageDirectory.isReadable())
        {
            result.failureReason = FileManagerBrowseFailureReason::IoFailure;
            result.errorMessage = QStringLiteral("Storage root cannot be read.");
            return result;
        }

        const QFileInfoList filesystemEntries = storageDirectory.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::NoSort);

        QList<MiniCloud::Protocol::FileEntryData> entries;

        for (const QFileInfo &filesystemEntry : filesystemEntries)
        {
            if (m_activeUploadTemporaryFilesystemPaths.contains(filesystemEntry.absoluteFilePath()))
            {
                continue;
            }

            MiniCloud::Protocol::FileEntryData entry;
            entry.name = filesystemEntry.fileName();
            entry.path = logicalPath == QStringLiteral("/") ? QStringLiteral("/") + entry.name : logicalPath + QLatin1Char('/') + entry.name;
            entry.lastModifiedUtcMs = static_cast<quint64>(filesystemEntry.lastModified(QTimeZone::UTC).toMSecsSinceEpoch());

            if (filesystemEntry.isDir())
            {
                entry.type = MiniCloud::Protocol::FileEntryType::Directory;
                entry.sizeBytes = 0;
            }
            else if (filesystemEntry.isFile())
            {
                entry.type = MiniCloud::Protocol::FileEntryType::File;
                entry.sizeBytes = static_cast<quint64>(filesystemEntry.size());
            }
            else
            {
                continue;
            }

            entries.append(entry);
        }

        std::sort(entries.begin(), entries.end(),
                  [](const MiniCloud::Protocol::FileEntryData &left, const MiniCloud::Protocol::FileEntryData &right)
                  {
                      if (left.type != right.type)
                      {
                          return left.type == MiniCloud::Protocol::FileEntryType::Directory;
                      }

                      return left.name < right.name;
                  });

        result.entries = entries;
        result.status = FileManagerOperationStatus::Success;
        return result;
    }

    FileManagerBrowseResult FileManager::search(const QString &directoryLogicalPath, const QString &query) const
    {
        if (query.trimmed().isEmpty())
        {
            FileManagerBrowseResult result;
            result.failureReason = FileManagerBrowseFailureReason::InvalidPath;
            result.errorMessage = QStringLiteral("Search query must not be blank.");
            return result;
        }

        FileManagerBrowseResult result = browse(directoryLogicalPath);

        if (result.status != FileManagerOperationStatus::Success)
        {
            return result;
        }

        QList<MiniCloud::Protocol::FileEntryData> matchingEntries;

        for (const MiniCloud::Protocol::FileEntryData &entry : result.entries)
        {
            if (entry.name.contains(query, Qt::CaseInsensitive))
            {
                matchingEntries.append(entry);
            }
        }

        result.entries = matchingEntries;
        return result;
    }

    FileManagerOperationResult FileManager::createDirectory(const QString &parentLogicalPath, const QString &name) const
    {
        FileManagerOperationResult result;

        if (!isCanonicalLogicalPath(parentLogicalPath))
        {
            result.errorMessage = QStringLiteral("Parent logical path must be canonical.");
            return result;
        }

        if (!isValidLeafName(name))
        {
            result.errorMessage =
                QStringLiteral("Directory name is invalid.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString parentRelativePath = parentLogicalPath.mid(1);

        const QString parentFilesystemPath = QDir(m_storageRoot).filePath(parentRelativePath);

        const QFileInfo parentDirectoryInfo(parentFilesystemPath);

        if (!parentDirectoryInfo.exists() || !parentDirectoryInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Parent logical path is not an existing directory.");
            return result;
        }

        QDir parentDirectory(parentFilesystemPath);

        if (!parentDirectory.mkdir(name))
        {
            result.errorMessage = QStringLiteral("Failed to create directory.");
            return result;
        }

        result.path = parentLogicalPath == QStringLiteral("/") ? QStringLiteral("/") + name : parentLogicalPath + QLatin1Char('/') + name;
        result.status = FileManagerOperationStatus::Success;
        return result;
    }

    FileManagerOperationResult FileManager::rename(const QString &sourceLogicalPath, const QString &newName) const
    {
        FileManagerOperationResult result;

        if (!isCanonicalLogicalPath(sourceLogicalPath) || sourceLogicalPath == QStringLiteral("/"))
        {
            result.errorMessage = QStringLiteral("Source logical path must identify an entry.");
            return result;
        }

        if (!isValidLeafName(newName))
        {
            result.errorMessage = QStringLiteral("New name is invalid.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString sourceRelativePath = sourceLogicalPath.mid(1);
        const QString sourceFilesystemPath = QDir(m_storageRoot).filePath(sourceRelativePath);
        const QFileInfo sourceInfo(sourceFilesystemPath);

        if (!sourceInfo.exists())
        {
            result.errorMessage = QStringLiteral("Source logical path does not exist.");
            return result;
        }

        const int lastSeparatorIndex = sourceLogicalPath.lastIndexOf(QLatin1Char('/'));
        const QString parentLogicalPath = sourceLogicalPath.left(lastSeparatorIndex);
        const QString sourceName = sourceLogicalPath.mid(lastSeparatorIndex + 1);

        const QString parentFilesystemPath = QDir(m_storageRoot).filePath(parentLogicalPath.mid(1));
        QDir parentDirectory(parentFilesystemPath);

        if (QFileInfo::exists(parentDirectory.filePath(newName)))
        {
            result.errorMessage = QStringLiteral("Destination entry already exists.");
            return result;
        }

        if (!parentDirectory.rename(sourceName, newName))
        {
            result.errorMessage = QStringLiteral("Failed to rename entry.");
            return result;
        }

        result.path = parentLogicalPath.isEmpty() ? QStringLiteral("/") + newName : parentLogicalPath + QLatin1Char('/') + newName;

        result.status = FileManagerOperationStatus::Success;
        return result;
    }

    FileManagerOperationResult FileManager::move(const QString &sourceLogicalPath, const QString &destinationDirectoryLogicalPath) const
    {
        FileManagerOperationResult result;

        if (!isCanonicalLogicalPath(sourceLogicalPath) || sourceLogicalPath == QStringLiteral("/"))
        {
            result.errorMessage = QStringLiteral("Source logical path must identify an entry.");
            return result;
        }

        if (!isCanonicalLogicalPath(destinationDirectoryLogicalPath))
        {
            result.errorMessage = QStringLiteral("Destination logical path must be canonical.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString sourceFilesystemPath = QDir(m_storageRoot).filePath(sourceLogicalPath.mid(1));
        const QFileInfo sourceInfo(sourceFilesystemPath);

        if (!sourceInfo.exists())
        {
            result.errorMessage = QStringLiteral("Source logical path does not exist.");
            return result;
        }

        if (sourceInfo.isDir() && (destinationDirectoryLogicalPath == sourceLogicalPath || destinationDirectoryLogicalPath.startsWith(sourceLogicalPath + QLatin1Char('/'))))
        {
            result.errorMessage = QStringLiteral("Destination directory cannot be the source or its descendant.");
            return result;
        }

        const QString destinationFilesystemPath = QDir(m_storageRoot).filePath(destinationDirectoryLogicalPath.mid(1));
        const QFileInfo destinationDirectoryInfo(destinationFilesystemPath);

        if (!destinationDirectoryInfo.exists() || !destinationDirectoryInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Destination logical path is not an existing directory.");
            return result;
        }

        const QString sourceName = sourceLogicalPath.mid(sourceLogicalPath.lastIndexOf(QLatin1Char('/')) + 1);
        const QString destinationEntryFilesystemPath = QDir(destinationFilesystemPath).filePath(sourceName);

        if (QFileInfo::exists(destinationEntryFilesystemPath))
        {
            result.errorMessage = QStringLiteral("Destination entry already exists.");
            return result;
        }

        QDir filesystemDirectory;

        if (!filesystemDirectory.rename(sourceFilesystemPath, destinationEntryFilesystemPath))
        {
            result.errorMessage = QStringLiteral("Failed to move entry.");
            return result;
        }

        result.path = destinationDirectoryLogicalPath == QStringLiteral("/") ? QStringLiteral("/") + sourceName : destinationDirectoryLogicalPath + QLatin1Char('/') + sourceName;
        result.status = FileManagerOperationStatus::Success;
        return result;
    }

    FileManagerOperationResult FileManager::remove(const QString &logicalPath) const
    {
        FileManagerOperationResult result;

        if (!isCanonicalLogicalPath(logicalPath) || logicalPath == QStringLiteral("/"))
        {
            result.errorMessage = QStringLiteral("Logical path must identify an entry.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString filesystemPath = QDir(m_storageRoot).filePath(logicalPath.mid(1));
        const QFileInfo entryInfo(filesystemPath);

        if (!entryInfo.exists())
        {
            result.errorMessage = QStringLiteral("Logical path does not exist.");
            return result;
        }

        bool removed = false;

        if (entryInfo.isFile())
        {
            removed = QFile::remove(filesystemPath);
        }
        else if (entryInfo.isDir())
        {
            const int lastSeparatorIndex = logicalPath.lastIndexOf(QLatin1Char('/'));
            const QString parentLogicalPath = logicalPath.left(lastSeparatorIndex);
            const QString entryName = logicalPath.mid(lastSeparatorIndex + 1);
            const QString parentFilesystemPath = QDir(m_storageRoot).filePath(parentLogicalPath.mid(1));
            const QDir parentDirectory(parentFilesystemPath);
            removed = parentDirectory.rmdir(entryName);
        }

        if (!removed)
        {
            result.errorMessage = QStringLiteral("Failed to remove entry.");
            return result;
        }

        result.path = logicalPath;
        result.status = FileManagerOperationStatus::Success;
        return result;
    }

    FileManagerUploadStartResult FileManager::beginUpload(const QString &destinationDirectoryPath,
                                                          const QString &fileName,
                                                          quint64 totalSizeBytes)
    {
        FileManagerUploadStartResult result;

        if (m_activeUploadFile)
        {
            result.errorMessage = QStringLiteral("An upload is already in progress.");
            return result;
        }

        if (!isCanonicalLogicalPath(destinationDirectoryPath))
        {
            result.errorMessage = QStringLiteral("Destination logical path must be canonical.");
            return result;
        }

        if (!isValidLeafName(fileName))
        {
            result.errorMessage = QStringLiteral("File name is invalid.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString destinationFilesystemPath = QDir(m_storageRoot).filePath(destinationDirectoryPath.mid(1));
        const QFileInfo destinationDirectoryInfo(destinationFilesystemPath);

        if (!destinationDirectoryInfo.exists() || !destinationDirectoryInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Destination logical path is not an existing directory.");
            return result;
        }

        const QString finalFilesystemPath = QDir(destinationFilesystemPath).filePath(fileName);

        if (QFileInfo::exists(finalFilesystemPath))
        {
            result.errorMessage = QStringLiteral("Destination entry already exists.");
            return result;
        }

        const QDir destinationDirectory(destinationFilesystemPath);

        const QStringList entryNamesBeforeUpload = destinationDirectory.entryList(
            QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden, QDir::NoSort);

        auto uploadFile = std::make_unique<QSaveFile>(finalFilesystemPath);

        if (!uploadFile->open(QIODevice::WriteOnly))
        {
            result.errorMessage = QStringLiteral("Failed to create temporary upload file.");
            return result;
        }

        m_activeUploadLogicalPath = destinationDirectoryPath == QStringLiteral("/")
                                        ? QStringLiteral("/") + fileName
                                        : destinationDirectoryPath + QLatin1Char('/') + fileName;

        m_activeUploadTotalSizeBytes = totalSizeBytes;
        m_activeUploadReceivedBytes = 0;
        m_activeUploadFile = std::move(uploadFile);

        if (totalSizeBytes == 0)
        {
            const QString completedLogicalPath = m_activeUploadLogicalPath;

            if (!m_activeUploadFile->commit())
            {
                result.errorMessage = QStringLiteral("Failed to commit uploaded file.");
                cancelActiveUpload();
                return result;
            }

            cancelActiveUpload();
            result.status = FileManagerOperationStatus::Success;
            result.completed = true;
            result.path = completedLogicalPath;
            return result;
        }

        const QFileInfoList entriesAfterStartingUpload = destinationDirectory.entryInfoList(
            QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden, QDir::NoSort);

        for (const QFileInfo &entry : entriesAfterStartingUpload)
        {
            if (!entryNamesBeforeUpload.contains(entry.fileName()))
            {
                m_activeUploadTemporaryFilesystemPaths.append(entry.absoluteFilePath());
            }
        }

        result.status = FileManagerOperationStatus::Success;
        result.path = m_activeUploadLogicalPath;
        return result;
    }

    FileManagerUploadChunkResult FileManager::appendUploadChunk(quint64 offset, const QByteArray &bytes)
    {
        FileManagerUploadChunkResult result;

        if (!m_activeUploadFile)
        {
            result.errorMessage = QStringLiteral("No upload is in progress.");
            return result;
        }

        if (offset != m_activeUploadReceivedBytes)
        {
            result.errorMessage = QStringLiteral("Upload chunk offset is invalid.");
            cancelActiveUpload();
            return result;
        }

        if (bytes.size() > maximumTransferChunkSize)
        {
            result.errorMessage = QStringLiteral("Upload chunk exceeds the 64 KiB limit.");
            cancelActiveUpload();
            return result;
        }

        const quint64 byteCount = static_cast<quint64>(bytes.size());

        if (byteCount > m_activeUploadTotalSizeBytes - m_activeUploadReceivedBytes)
        {
            result.errorMessage = QStringLiteral("Upload chunk exceeds the declared file size.");
            cancelActiveUpload();
            return result;
        }

        if (m_activeUploadFile->write(bytes) != bytes.size())
        {
            result.errorMessage = QStringLiteral("Failed to write upload chunk.");
            cancelActiveUpload();
            return result;
        }

        m_activeUploadReceivedBytes += byteCount;

        if (m_activeUploadReceivedBytes < m_activeUploadTotalSizeBytes)
        {
            result.status = FileManagerOperationStatus::Success;
            return result;
        }

        const QString completedLogicalPath = m_activeUploadLogicalPath;

        if (!m_activeUploadFile->commit())
        {
            result.errorMessage = QStringLiteral("Failed to commit uploaded file.");
            cancelActiveUpload();
            return result;
        }

        cancelActiveUpload();
        result.status = FileManagerOperationStatus::Success;
        result.completed = true;
        result.path = completedLogicalPath;
        return result;
    }

    void FileManager::cancelUpload()
    {
        cancelActiveUpload();
    }

    FileManagerDownloadStartResult FileManager::beginDownload(const QString &logicalPath)
    {
        FileManagerDownloadStartResult result;

        if (m_activeDownloadFile)
        {
            result.failureReason = FileManagerDownloadFailureReason::TransferInProgress;
            result.errorMessage = QStringLiteral("A download is already in progress.");
            return result;
        }

        if (!isCanonicalLogicalPath(logicalPath) || logicalPath == QStringLiteral("/"))
        {
            result.failureReason = FileManagerDownloadFailureReason::InvalidPath;
            result.errorMessage = QStringLiteral("Logical path must identify a file.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.failureReason = FileManagerDownloadFailureReason::IoFailure;
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString filesystemPath = QDir(m_storageRoot).filePath(logicalPath.mid(1));
        const QFileInfo sourceInfo(filesystemPath);

        if (!sourceInfo.exists() || !sourceInfo.isFile())
        {
            result.failureReason = FileManagerDownloadFailureReason::NotFound;
            result.errorMessage = QStringLiteral("Logical path is not an existing file.");
            return result;
        }

        auto downloadFile = std::make_unique<QFile>(filesystemPath);

        if (!downloadFile->open(QIODevice::ReadOnly))
        {
            result.failureReason = FileManagerDownloadFailureReason::IoFailure;
            result.errorMessage = QStringLiteral("Failed to open file for download.");
            return result;
        }

        result.path = logicalPath;
        result.totalSizeBytes = static_cast<quint64>(sourceInfo.size());

        if (result.totalSizeBytes == 0)
        {
            result.status = FileManagerOperationStatus::Success;
            result.completed = true;
            return result;
        }

        m_activeDownloadFile = std::move(downloadFile);
        m_activeDownloadLogicalPath = logicalPath;
        m_activeDownloadTotalSizeBytes = result.totalSizeBytes;
        m_activeDownloadReadBytes = 0;
        result.status = FileManagerOperationStatus::Success;
        return result;
    }

    FileManagerDownloadChunkResult FileManager::readNextDownloadChunk()
    {
        FileManagerDownloadChunkResult result;

        if (!m_activeDownloadFile)
        {
            result.errorMessage = QStringLiteral("No download is in progress.");
            return result;
        }

        const quint64 remainingBytes = m_activeDownloadTotalSizeBytes - m_activeDownloadReadBytes;

        const qsizetype readSize = static_cast<qsizetype>(
            std::min<quint64>(static_cast<quint64>(maximumTransferChunkSize), remainingBytes));

        const QByteArray data = m_activeDownloadFile->read(readSize);

        if (data.isEmpty() && remainingBytes > 0)
        {
            result.errorMessage = QStringLiteral("Failed to read the next download chunk.");
            cancelActiveDownload();
            return result;
        }

        result.offset = m_activeDownloadReadBytes;
        result.data = data;
        m_activeDownloadReadBytes += static_cast<quint64>(data.size());

        if (m_activeDownloadReadBytes < m_activeDownloadTotalSizeBytes)
        {
            if (m_activeDownloadFile->atEnd())
            {
                result.errorMessage = QStringLiteral("Downloaded file became shorter than its declared size.");
                result.data.clear();
                result.offset = 0;
                cancelActiveDownload();
                return result;
            }

            result.status = FileManagerOperationStatus::Success;
            return result;
        }

        cancelActiveDownload();
        result.status = FileManagerOperationStatus::Success;
        result.completed = true;
        return result;
    }

    void FileManager::cancelActiveUpload()
    {
        m_activeUploadFile.reset();
        m_activeUploadLogicalPath.clear();
        m_activeUploadTemporaryFilesystemPaths.clear();
        m_activeUploadTotalSizeBytes = 0;
        m_activeUploadReceivedBytes = 0;
    }

    void FileManager::cancelActiveDownload()
    {
        m_activeDownloadFile.reset();
        m_activeDownloadLogicalPath.clear();
        m_activeDownloadTotalSizeBytes = 0;
        m_activeDownloadReadBytes = 0;
    }
}
