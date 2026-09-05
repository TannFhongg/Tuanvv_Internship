#include "filemanager.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTimeZone>

#include <algorithm>
#include <utility>

namespace
{
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
            result.errorMessage = QStringLiteral("Logical path must be canonical.");
            return result;
        }

        const QFileInfo storageRootInfo(m_storageRoot);

        if (!storageRootInfo.exists() || !storageRootInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Storage root is not an existing directory.");
            return result;
        }

        const QString relativePath = logicalPath.mid(1);

        const QString filesystemPath = QDir(m_storageRoot).filePath(relativePath);
        const QFileInfo browseDirectoryInfo(filesystemPath);

        if (!browseDirectoryInfo.exists() || !browseDirectoryInfo.isDir())
        {
            result.errorMessage = QStringLiteral("Logical path is not an existing directory.");
            return result;
        }

        const QDir storageDirectory(filesystemPath);

        if (!storageDirectory.isReadable())
        {
            result.errorMessage = QStringLiteral("Storage root cannot be read.");
            return result;
        }

        const QFileInfoList filesystemEntries = storageDirectory.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::NoSort);

        QList<MiniCloud::Protocol::FileEntryData> entries;

        for (const QFileInfo &filesystemEntry : filesystemEntries)
        {
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

        const QDir parentDirectory(parentFilesystemPath);

        if (!parentDirectory.mkdir(name))
        {
            result.errorMessage = QStringLiteral("Failed to create directory.");
            return result;
        }

        result.path = parentLogicalPath == QStringLiteral("/") ? QStringLiteral("/") + name : parentLogicalPath + QLatin1Char('/') + name;
        result.status = FileManagerOperationStatus::Success;
        return result;
    }
}
