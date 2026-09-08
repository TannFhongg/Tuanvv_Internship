#include <QtTest/QTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimeZone>

#include "filemanager.h"

class FileManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void browseRoot_emptyStorage_returnsEmptyEntries()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.browse(QStringLiteral("/"));
        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(result.entries.isEmpty());

        const QDir storageDirectory(storageRoot);
        QCOMPARE(storageDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries), QStringList());
    }

    void browseRoot_multipleEntries_returnsAllMetadataInDeterministicOrder()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Projects")));
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Archive")));

        const QString zetaPath = storageDirectory.filePath(QStringLiteral("zeta.txt"));
        QFile zetaFile(zetaPath);
        QVERIFY(zetaFile.open(QIODevice::WriteOnly));
        QCOMPARE(zetaFile.write("zeta"), qint64{4});
        zetaFile.close();

        const QString alphaPath = storageDirectory.filePath(QStringLiteral("alpha.txt"));
        QFile alphaFile(alphaPath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("abc"), qint64{3});
        alphaFile.close();

        const QFileInfo archiveInfo(storageDirectory.filePath(QStringLiteral("Archive")));
        const QFileInfo projectsInfo(storageDirectory.filePath(QStringLiteral("Projects")));
        const QFileInfo alphaInfo(alphaPath);
        const QFileInfo zetaInfo(zetaPath);

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.browse(QStringLiteral("/"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.entries.size(), 4);

        const FileEntryData &archive = result.entries.at(0);
        QCOMPARE(archive.name, QStringLiteral("Archive"));
        QCOMPARE(archive.path, QStringLiteral("/Archive"));
        QCOMPARE(archive.type, FileEntryType::Directory);
        QCOMPARE(archive.sizeBytes, quint64{0});
        QCOMPARE(archive.lastModifiedUtcMs, static_cast<quint64>(archiveInfo.lastModified(QTimeZone::UTC).toMSecsSinceEpoch()));

        const FileEntryData &projects = result.entries.at(1);
        QCOMPARE(projects.name, QStringLiteral("Projects"));
        QCOMPARE(projects.path, QStringLiteral("/Projects"));
        QCOMPARE(projects.type, FileEntryType::Directory);
        QCOMPARE(projects.sizeBytes, quint64{0});
        QCOMPARE(projects.lastModifiedUtcMs, static_cast<quint64>(projectsInfo.lastModified(QTimeZone::UTC).toMSecsSinceEpoch()));

        const FileEntryData &alpha = result.entries.at(2);
        QCOMPARE(alpha.name, QStringLiteral("alpha.txt"));
        QCOMPARE(alpha.path, QStringLiteral("/alpha.txt"));
        QCOMPARE(alpha.type, FileEntryType::File);
        QCOMPARE(alpha.sizeBytes, static_cast<quint64>(alphaInfo.size()));
        QCOMPARE(alpha.lastModifiedUtcMs, static_cast<quint64>(alphaInfo.lastModified(QTimeZone::UTC).toMSecsSinceEpoch()));

        const FileEntryData &zeta = result.entries.at(3);
        QCOMPARE(zeta.name, QStringLiteral("zeta.txt"));
        QCOMPARE(zeta.path, QStringLiteral("/zeta.txt"));
        QCOMPARE(zeta.type, FileEntryType::File);
        QCOMPARE(zeta.sizeBytes, static_cast<quint64>(zetaInfo.size()));
        QCOMPARE(zeta.lastModifiedUtcMs, static_cast<quint64>(zetaInfo.lastModified(QTimeZone::UTC).toMSecsSinceEpoch()));
    }

    void browseNestedDirectory_returnsOnlyDirectChildrenWithLogicalPaths()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Documents")));

        const QString rootOnlyPath = storageDirectory.filePath(QStringLiteral("root-only.txt"));
        QFile rootOnlyFile(rootOnlyPath);
        QVERIFY(rootOnlyFile.open(QIODevice::WriteOnly));
        QCOMPARE(rootOnlyFile.write("root"), qint64{4});
        rootOnlyFile.close();

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString alphaPath = documentsDirectory.filePath(QStringLiteral("alpha.txt"));
        QFile alphaFile(alphaPath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("abc"), qint64{3});
        alphaFile.close();

        QVERIFY(documentsDirectory.mkdir(QStringLiteral("Reports")));

        const QDir reportsDirectory(documentsDirectory.filePath(QStringLiteral("Reports")));
        const QString internalPath = reportsDirectory.filePath(QStringLiteral("internal.txt"));
        QFile internalFile(internalPath);
        QVERIFY(internalFile.open(QIODevice::WriteOnly));
        QCOMPARE(internalFile.write("internal"), qint64{8});
        internalFile.close();

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.browse(QStringLiteral("/Documents"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.entries.size(), 2);

        const FileEntryData &reports = result.entries.at(0);
        QCOMPARE(reports.name, QStringLiteral("Reports"));
        QCOMPARE(reports.path, QStringLiteral("/Documents/Reports"));
        QCOMPARE(reports.type, FileEntryType::Directory);

        const FileEntryData &alpha = result.entries.at(1);
        QCOMPARE(alpha.name, QStringLiteral("alpha.txt"));
        QCOMPARE(alpha.path, QStringLiteral("/Documents/alpha.txt"));
        QCOMPARE(alpha.type, FileEntryType::File);

        for (const FileEntryData &entry : result.entries)
        {
            QVERIFY(entry.name != QStringLiteral("root-only.txt"));
            QVERIFY(entry.name != QStringLiteral("internal.txt"));
        }
    }

    void browsePathTraversal_failsWithoutExposingOutsideStorage_data()
    {
        QTest::addColumn<QString>("logicalPath");

        QTest::newRow("leaves-root") << QStringLiteral("/../outside");
        QTest::newRow("nested-leaves-root") << QStringLiteral("/Documents/../../outside");
        QTest::newRow("stays-inside-but-uses-traversal") << QStringLiteral("/Documents/..");
    }

    void browsePathTraversal_failsWithoutExposingOutsideStorage()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, logicalPath);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString outsideRoot = temporaryDirectory.filePath(QStringLiteral("outside"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));
        QVERIFY(QDir().mkpath(outsideRoot));

        const QString outsideSecretPath = QDir(outsideRoot).filePath(QStringLiteral("secret.txt"));
        QFile outsideSecretFile(outsideSecretPath);
        QVERIFY(outsideSecretFile.open(QIODevice::WriteOnly));
        QCOMPARE(outsideSecretFile.write("secret"), qint64{6});
        outsideSecretFile.close();

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.browse(logicalPath);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.entries.isEmpty());

        QVERIFY(QFileInfo::exists(outsideSecretPath));
    }

    void browseNonCanonicalLogicalPath_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("logicalPath");

        QTest::newRow("empty") << QString();
        QTest::newRow("whitespace") << QStringLiteral("   ");
        QTest::newRow("relative-path") << QStringLiteral("Documents");
        QTest::newRow("windows-absolute-path") << QStringLiteral("C:/Windows");
        QTest::newRow("backslash-separator") << QStringLiteral("/Documents\\Reports");
        QTest::newRow("dot-segment") << QStringLiteral("/Documents/./Reports");
        QTest::newRow("duplicate-separator") << QStringLiteral("/Documents//Reports");
        QTest::newRow("trailing-separator") << QStringLiteral("/Documents/");
    }

    void browseNonCanonicalLogicalPath_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;
        QFETCH(QString, logicalPath);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        const QString fixturePath = QDir(storageRoot).filePath(QStringLiteral("fixture.txt"));
        QFile fixtureFile(fixturePath);
        QVERIFY(fixtureFile.open(QIODevice::WriteOnly));
        QCOMPARE(fixtureFile.write("fixture"), qint64{7});
        fixtureFile.close();

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.browse(logicalPath);
        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.entries.isEmpty());

        QVERIFY(QFileInfo::exists(fixturePath));
    }

    void browseMissingOrFilePath_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("logicalPath");

        QTest::newRow("missing-directory") << QStringLiteral("/Missing");
        QTest::newRow("path-is-file") << QStringLiteral("/Documents/report.txt");
    }

    void browseMissingOrFilePath_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, logicalPath);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkpath(QStringLiteral("Documents")));

        const QString reportFilePath = QDir(storageDirectory.filePath(QStringLiteral("Documents"))).filePath(QStringLiteral("report.txt"));
        QFile reportFile(reportFilePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.browse(logicalPath);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.entries.isEmpty());

        QVERIFY(QFileInfo::exists(reportFilePath));
        QVERIFY(QFileInfo(reportFilePath).isFile());
    }

    void createDirectory_underExistingDirectory_createsAndReturnsLogicalPath()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.createDirectory(QStringLiteral("/Documents"), QStringLiteral("Projects"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Documents/Projects"));

        const QString nativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"));

        QVERIFY(QFileInfo(nativePath).isDir());

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));

        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(browseResult.entries.size(), 1);

        QCOMPARE(browseResult.entries.at(0).name, QStringLiteral("Projects"));
        QCOMPARE(browseResult.entries.at(0).path, QStringLiteral("/Documents/Projects"));
        QCOMPARE(browseResult.entries.at(0).type, FileEntryType::Directory);
    }

    void createDirectory_invalidPathOrName_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("parentPath");
        QTest::addColumn<QString>("name");

        QTest::newRow("blank-parent") << QString() << QStringLiteral("Projects");
        QTest::newRow("traversal-parent") << QStringLiteral("/../outside") << QStringLiteral("Projects");
        QTest::newRow("relative-parent") << QStringLiteral("Documents") << QStringLiteral("Projects");
        QTest::newRow("blank-name") << QStringLiteral("/Documents") << QString();
        QTest::newRow("whitespace-name") << QStringLiteral("/Documents") << QStringLiteral("   ");
        QTest::newRow("dot-name") << QStringLiteral("/Documents") << QStringLiteral(".");
        QTest::newRow("dotdot-name") << QStringLiteral("/Documents") << QStringLiteral("..");
        QTest::newRow("slash-in-name") << QStringLiteral("/Documents") << QStringLiteral("Nested/Projects");
        QTest::newRow("backslash-in-name") << QStringLiteral("/Documents") << QStringLiteral("Nested\\Projects");
    }

    void createDirectory_invalidPathOrName_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, parentPath);
        QFETCH(QString, name);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.createDirectory(parentPath, name);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        const QDir storageDirectory(storageRoot);
        QCOMPARE(storageDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries), QStringList{QStringLiteral("Documents")});

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        QCOMPARE(documentsDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries), QStringList());
    }

    void createDirectory_atRoot_createsAndReturnsRootChild()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(storageRoot));

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.createDirectory(QStringLiteral("/"), QStringLiteral("Documents"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Documents"));

        const QString nativePath = QDir(storageRoot).filePath(QStringLiteral("Documents"));

        QVERIFY(QFileInfo(nativePath).isDir());

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/"));

        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(browseResult.entries.size(), 1);
        QCOMPARE(browseResult.entries.at(0).name, QStringLiteral("Documents"));
        QCOMPARE(browseResult.entries.at(0).path, QStringLiteral("/Documents"));
        QCOMPARE(browseResult.entries.at(0).type, FileEntryType::Directory);
    }

    void createDirectory_missingOrFileParent_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("parentPath");

        QTest::newRow("missing-parent") << QStringLiteral("/Missing");
        QTest::newRow("file-parent") << QStringLiteral("/Documents/report.txt");
    }

    void createDirectory_missingOrFileParent_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, parentPath);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkpath(QStringLiteral("Documents")));

        const QString reportFilePath = QDir(storageDirectory.filePath(QStringLiteral("Documents"))).filePath(QStringLiteral("report.txt"));
        QFile reportFile(reportFilePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.createDirectory(parentPath, QStringLiteral("Projects"));

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(!QFileInfo::exists(QDir(storageRoot).filePath(QStringLiteral("Missing/Projects"))));
        QVERIFY(!QFileInfo::exists(QDir(storageRoot).filePath(QStringLiteral("Documents/report.txt/Projects"))));
        QVERIFY(!QFileInfo::exists(QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"))));
        QVERIFY(QFileInfo(reportFilePath).isFile());
    }

    void createDirectory_existingFileOrDirectory_failsWithoutOverwriting_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("existingIsDirectory");

        QTest::newRow("existing-directory") << QStringLiteral("Projects") << true;
        QTest::newRow("existing-file") << QStringLiteral("readme.txt") << false;
    }

    void createDirectory_existingFileOrDirectory_failsWithoutOverwriting()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, name);
        QFETCH(bool, existingIsDirectory);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkpath(QStringLiteral("Documents")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString existingNativePath = documentsDirectory.filePath(name);

        if (existingIsDirectory)
        {
            QVERIFY(documentsDirectory.mkdir(name));
        }
        else
        {
            QFile existingFile(existingNativePath);
            QVERIFY(existingFile.open(QIODevice::WriteOnly));
            QCOMPARE(existingFile.write("existing"), qint64{8});
            existingFile.close();
        }

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.createDirectory(QStringLiteral("/Documents"), name);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        const QFileInfo existingInfo(existingNativePath);

        QVERIFY(existingInfo.exists());
        QCOMPARE(existingInfo.isDir(), existingIsDirectory);
    }

    void searchCurrentDirectory_caseInsensitiveSubstring_returnsSortedDirectMatches()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkpath(QStringLiteral("Documents")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        QVERIFY(documentsDirectory.mkdir(QStringLiteral("Reports")));
        QVERIFY(documentsDirectory.mkdir(QStringLiteral("Archive")));

        const QString reportPath = documentsDirectory.filePath(QStringLiteral("Report-2026.txt"));
        QFile reportFile(reportPath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        const QString notesPath = documentsDirectory.filePath(QStringLiteral("report-notes.md"));
        QFile notesFile(notesPath);
        QVERIFY(notesFile.open(QIODevice::WriteOnly));
        QCOMPARE(notesFile.write("notes"), qint64{5});
        notesFile.close();

        const QString imagePath = documentsDirectory.filePath(QStringLiteral("image.png"));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::WriteOnly));
        QCOMPARE(imageFile.write("image"), qint64{5});
        imageFile.close();

        const QDir archiveDirectory(documentsDirectory.filePath(QStringLiteral("Archive")));
        const QString quarterlyReportPath = archiveDirectory.filePath(QStringLiteral("quarterly-report.txt"));
        QFile quarterlyReportFile(quarterlyReportPath);
        QVERIFY(quarterlyReportFile.open(QIODevice::WriteOnly));
        QCOMPARE(quarterlyReportFile.write("quarterly"), qint64{9});
        quarterlyReportFile.close();

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.search(
            QStringLiteral("/Documents"),
            QStringLiteral("RePoRt"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.entries.size(), 3);

        const FileEntryData &reports = result.entries.at(0);
        QCOMPARE(reports.name, QStringLiteral("Reports"));
        QCOMPARE(reports.path, QStringLiteral("/Documents/Reports"));
        QCOMPARE(reports.type, FileEntryType::Directory);

        const FileEntryData &report = result.entries.at(1);
        QCOMPARE(report.name, QStringLiteral("Report-2026.txt"));
        QCOMPARE(report.path, QStringLiteral("/Documents/Report-2026.txt"));
        QCOMPARE(report.type, FileEntryType::File);

        const FileEntryData &notes = result.entries.at(2);
        QCOMPARE(notes.name, QStringLiteral("report-notes.md"));
        QCOMPARE(notes.path, QStringLiteral("/Documents/report-notes.md"));
        QCOMPARE(notes.type, FileEntryType::File);

        for (const FileEntryData &entry : result.entries)
        {
            QVERIFY(entry.name != QStringLiteral("quarterly-report.txt"));
        }
    }

    void searchNoMatches_returnsSuccessWithEmptyEntries()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkpath(QStringLiteral("Documents")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString alphaPath = documentsDirectory.filePath(QStringLiteral("alpha.txt"));
        QFile alphaFile(alphaPath);
        QVERIFY(alphaFile.open(QIODevice::WriteOnly));
        QCOMPARE(alphaFile.write("alpha"), qint64{5});
        alphaFile.close();

        const QString reportsPath = documentsDirectory.filePath(QStringLiteral("reports"));
        QVERIFY(documentsDirectory.mkdir(QStringLiteral("reports")));

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.search(
            QStringLiteral("/Documents"),
            QStringLiteral("not-found"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(result.entries.isEmpty());

        QVERIFY(QFileInfo::exists(alphaPath));
        QVERIFY(QFileInfo::exists(reportsPath));
    }

    void searchInvalidDirectoryOrQuery_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("directoryPath");
        QTest::addColumn<QString>("query");

        QTest::newRow("empty-query") << QStringLiteral("/Documents") << QString();
        QTest::newRow("blank-query") << QStringLiteral("/Documents") << QStringLiteral("   ");
        QTest::newRow("missing-directory") << QStringLiteral("/Missing") << QStringLiteral("report");
        QTest::newRow("path-is-file") << QStringLiteral("/Documents/report.txt") << QStringLiteral("report");
        QTest::newRow("traversal-directory") << QStringLiteral("/../outside") << QStringLiteral("report");
    }

    void searchInvalidDirectoryOrQuery_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, directoryPath);
        QFETCH(QString, query);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkpath(QStringLiteral("Documents")));

        const QString reportFilePath = QDir(storageDirectory.filePath(QStringLiteral("Documents"))).filePath(QStringLiteral("report.txt"));
        QFile reportFile(reportFilePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerBrowseResult result = manager.search(directoryPath, query);
        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.entries.isEmpty());

        QVERIFY(QFileInfo::exists(reportFilePath));
    }

    void renameExistingFile_preservesContentAndReturnsNewLogicalPath()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        const QString oldNativePath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile reportFile(oldNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("quarterly-data"), qint64{14});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.rename(QStringLiteral("/Documents/report.txt"), QStringLiteral("summary.txt"));
        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Documents/summary.txt"));

        const QString newNativePath = documentsDirectory.filePath(QStringLiteral("summary.txt"));
        QVERIFY(!QFileInfo::exists(oldNativePath));
        QVERIFY(QFileInfo::exists(newNativePath));
        QVERIFY(QFileInfo(newNativePath).isFile());

        QFile renamedFile(newNativePath);
        QVERIFY(renamedFile.open(QIODevice::ReadOnly));
        QCOMPARE(renamedFile.readAll(), QByteArrayLiteral("quarterly-data"));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(browseResult.entries.size(), 1);
        QCOMPARE(browseResult.entries.at(0).name, QStringLiteral("summary.txt"));
        QCOMPARE(browseResult.entries.at(0).path, QStringLiteral("/Documents/summary.txt"));
        QCOMPARE(browseResult.entries.at(0).type, FileEntryType::File);
    }

    void renameExistingDirectory_preservesChildrenAndReturnsNewLogicalPath()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"))));

        const QString oldDirectoryPath = QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"));
        const QString planPath = QDir(oldDirectoryPath).filePath(QStringLiteral("plan.txt"));
        QFile planFile(planPath);
        QVERIFY(planFile.open(QIODevice::WriteOnly));
        QCOMPARE(planFile.write("project-plan"), qint64{12});
        planFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.rename(QStringLiteral("/Documents/Projects"), QStringLiteral("Archive"));
        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Documents/Archive"));

        const QString newDirectoryPath = QDir(storageRoot).filePath(QStringLiteral("Documents/Archive"));
        const QString renamedPlanPath = QDir(newDirectoryPath).filePath(QStringLiteral("plan.txt"));
        QVERIFY(!QFileInfo::exists(oldDirectoryPath));
        QVERIFY(QFileInfo(newDirectoryPath).isDir());
        QVERIFY(QFileInfo(renamedPlanPath).isFile());

        QFile renamedPlanFile(renamedPlanPath);
        QVERIFY(renamedPlanFile.open(QIODevice::ReadOnly));
        QCOMPARE(renamedPlanFile.readAll(), QByteArrayLiteral("project-plan"));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(browseResult.entries.size(), 1);
        QCOMPARE(browseResult.entries.at(0).name, QStringLiteral("Archive"));
        QCOMPARE(browseResult.entries.at(0).path, QStringLiteral("/Documents/Archive"));
        QCOMPARE(browseResult.entries.at(0).type, FileEntryType::Directory);
    }

    void renameInvalidSourceOrName_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("sourcePath");
        QTest::addColumn<QString>("newName");

        QTest::newRow("missing-source") << QStringLiteral("/Documents/missing.txt") << QStringLiteral("renamed.txt");
        QTest::newRow("root-source") << QStringLiteral("/") << QStringLiteral("renamed-root");
        QTest::newRow("traversal-source") << QStringLiteral("/../outside") << QStringLiteral("renamed.txt");
        QTest::newRow("blank-new-name") << QStringLiteral("/Documents/report.txt") << QString();
        QTest::newRow("whitespace-new-name") << QStringLiteral("/Documents/report.txt") << QStringLiteral("   ");
        QTest::newRow("dot-new-name") << QStringLiteral("/Documents/report.txt") << QStringLiteral(".");
        QTest::newRow("dotdot-new-name") << QStringLiteral("/Documents/report.txt") << QStringLiteral("..");
        QTest::newRow("slash-in-new-name") << QStringLiteral("/Documents/report.txt") << QStringLiteral("nested/renamed.txt");
        QTest::newRow("backslash-in-new-name") << QStringLiteral("/Documents/report.txt") << QStringLiteral("nested\\renamed.txt");
    }

    void renameInvalidSourceOrName_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, sourcePath);
        QFETCH(QString, newName);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString reportPath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.txt"));
        QFile reportFile(reportPath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report-data"), qint64{11});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.rename(sourcePath, newName);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo::exists(reportPath));
        QVERIFY(QFileInfo(reportPath).isFile());

        QFile unchangedReportFile(reportPath);
        QVERIFY(unchangedReportFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedReportFile.readAll(), QByteArrayLiteral("report-data"));
    }

    void renameToExistingFileOrDirectory_failsWithoutOverwriting_data()
    {
        QTest::addColumn<QString>("newName");
        QTest::addColumn<bool>("targetIsDirectory");

        QTest::newRow("target-is-file") << QStringLiteral("summary.txt") << false;
        QTest::newRow("target-is-directory") << QStringLiteral("Archive") << true;
    }

    void renameToExistingFileOrDirectory_failsWithoutOverwriting()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, newName);
        QFETCH(bool, targetIsDirectory);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));

        const QString sourcePath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile sourceFile(sourcePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write("source-data"), qint64{11});
        sourceFile.close();

        const QString targetFilePath = documentsDirectory.filePath(QStringLiteral("summary.txt"));
        QFile targetFile(targetFilePath);
        QVERIFY(targetFile.open(QIODevice::WriteOnly));
        QCOMPARE(targetFile.write("target-data"), qint64{11});
        targetFile.close();

        const QString targetDirectoryPath = documentsDirectory.filePath(QStringLiteral("Archive"));
        QVERIFY(documentsDirectory.mkdir(QStringLiteral("Archive")));

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.rename(QStringLiteral("/Documents/report.txt"), newName);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo(sourcePath).isFile());
        QFile unchangedSourceFile(sourcePath);
        QVERIFY(unchangedSourceFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedSourceFile.readAll(), QByteArrayLiteral("source-data"));

        const QFileInfo targetInfo(documentsDirectory.filePath(newName));
        QVERIFY(targetInfo.exists());
        QCOMPARE(targetInfo.isDir(), targetIsDirectory);

        if (!targetIsDirectory)
        {
            QFile unchangedTargetFile(targetFilePath);
            QVERIFY(unchangedTargetFile.open(QIODevice::ReadOnly));
            QCOMPARE(unchangedTargetFile.readAll(), QByteArrayLiteral("target-data"));
        }

        const QStringList expectedEntries{
            QStringLiteral("Archive"),
            QStringLiteral("report.txt"),
            QStringLiteral("summary.txt")};
        QCOMPARE(documentsDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name), expectedEntries);
        QVERIFY(QFileInfo(targetDirectoryPath).isDir());
    }

    void moveExistingFile_toDifferentDirectory_preservesContentAndReturnsDestinationPath()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Archive")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString oldNativePath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile reportFile(oldNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("quarterly-data"), qint64{14});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.move(
            QStringLiteral("/Documents/report.txt"),
            QStringLiteral("/Archive"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Archive/report.txt"));

        const QString newNativePath = storageDirectory.filePath(QStringLiteral("Archive/report.txt"));
        QVERIFY(!QFileInfo::exists(oldNativePath));
        QVERIFY(QFileInfo(newNativePath).isFile());

        QFile movedFile(newNativePath);
        QVERIFY(movedFile.open(QIODevice::ReadOnly));
        QCOMPARE(movedFile.readAll(), QByteArrayLiteral("quarterly-data"));

        const FileManagerBrowseResult documentsBrowseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(documentsBrowseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(documentsBrowseResult.entries.isEmpty());

        const FileManagerBrowseResult archiveBrowseResult = manager.browse(QStringLiteral("/Archive"));
        QCOMPARE(archiveBrowseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(archiveBrowseResult.entries.size(), 1);
        QCOMPARE(archiveBrowseResult.entries.at(0).name, QStringLiteral("report.txt"));
        QCOMPARE(archiveBrowseResult.entries.at(0).path, QStringLiteral("/Archive/report.txt"));
        QCOMPARE(archiveBrowseResult.entries.at(0).type, FileEntryType::File);
    }

    void moveExistingDirectory_toDifferentDirectory_preservesDescendantsAndReturnsDestinationPath()
    {
        using namespace MiniCloud::Protocol;
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"))));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Archive")));

        const QString oldDirectoryPath = storageDirectory.filePath(QStringLiteral("Documents/Projects"));
        const QString planPath = QDir(oldDirectoryPath).filePath(QStringLiteral("plan.txt"));
        QFile planFile(planPath);
        QVERIFY(planFile.open(QIODevice::WriteOnly));
        QCOMPARE(planFile.write("project-plan"), qint64{12});
        planFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.move(
            QStringLiteral("/Documents/Projects"),
            QStringLiteral("/Archive"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Archive/Projects"));

        const QString newDirectoryPath = storageDirectory.filePath(QStringLiteral("Archive/Projects"));
        const QString movedChildPath = QDir(newDirectoryPath).filePath(QStringLiteral("plan.txt"));
        QVERIFY(!QFileInfo::exists(oldDirectoryPath));
        QVERIFY(QFileInfo(newDirectoryPath).isDir());
        QVERIFY(QFileInfo(movedChildPath).isFile());

        QFile movedChildFile(movedChildPath);
        QVERIFY(movedChildFile.open(QIODevice::ReadOnly));
        QCOMPARE(movedChildFile.readAll(), QByteArrayLiteral("project-plan"));

        const FileManagerBrowseResult documentsBrowseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(documentsBrowseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(documentsBrowseResult.entries.isEmpty());

        const FileManagerBrowseResult archiveBrowseResult = manager.browse(QStringLiteral("/Archive"));
        QCOMPARE(archiveBrowseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(archiveBrowseResult.entries.size(), 1);
        QCOMPARE(archiveBrowseResult.entries.at(0).name, QStringLiteral("Projects"));
        QCOMPARE(archiveBrowseResult.entries.at(0).path, QStringLiteral("/Archive/Projects"));
        QCOMPARE(archiveBrowseResult.entries.at(0).type, FileEntryType::Directory);
    }

    void moveInvalidSourceOrDestination_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("sourcePath");
        QTest::addColumn<QString>("destinationDirectoryPath");

        QTest::newRow("missing-source") << QStringLiteral("/Documents/missing.txt") << QStringLiteral("/Archive");
        QTest::newRow("root-source") << QStringLiteral("/") << QStringLiteral("/Archive");
        QTest::newRow("traversal-source") << QStringLiteral("/../outside") << QStringLiteral("/Archive");
        QTest::newRow("missing-destination") << QStringLiteral("/Documents/report.txt") << QStringLiteral("/Missing");
        QTest::newRow("destination-is-file") << QStringLiteral("/Documents/report.txt") << QStringLiteral("/Documents/target.txt");
        QTest::newRow("traversal-destination") << QStringLiteral("/Documents/report.txt") << QStringLiteral("/../outside");
    }

    void moveInvalidSourceOrDestination_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, sourcePath);
        QFETCH(QString, destinationDirectoryPath);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Archive")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString reportPath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile reportFile(reportPath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("source-data"), qint64{11});
        reportFile.close();

        const QString targetPath = documentsDirectory.filePath(QStringLiteral("target.txt"));
        QFile targetFile(targetPath);
        QVERIFY(targetFile.open(QIODevice::WriteOnly));
        QCOMPARE(targetFile.write("target-data"), qint64{11});
        targetFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.move(sourcePath, destinationDirectoryPath);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo(reportPath).isFile());
        QFile unchangedReportFile(reportPath);
        QVERIFY(unchangedReportFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedReportFile.readAll(), QByteArrayLiteral("source-data"));

        QVERIFY(QFileInfo(targetPath).isFile());
        const QDir archiveDirectory(storageDirectory.filePath(QStringLiteral("Archive")));
        QVERIFY(archiveDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty());
    }

    void moveDirectoryIntoOwnDescendant_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Projects/Child"))));

        const QDir storageDirectory(storageRoot);
        const QString projectsPath = storageDirectory.filePath(QStringLiteral("Documents/Projects"));
        const QString childPath = QDir(projectsPath).filePath(QStringLiteral("Child"));
        const QString planFilePath = QDir(childPath).filePath(QStringLiteral("plan.txt"));
        QFile planFile(planFilePath);
        QVERIFY(planFile.open(QIODevice::WriteOnly));
        QCOMPARE(planFile.write("plan"), qint64{4});
        planFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.move(
            QStringLiteral("/Documents/Projects"),
            QStringLiteral("/Documents/Projects/Child"));

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo(projectsPath).isDir());
        QVERIFY(QFileInfo(childPath).isDir());
        QVERIFY(QFileInfo(planFilePath).isFile());
    }

    void moveToDestinationWithSameLeafName_failsWithoutOverwriting_data()
    {
        QTest::addColumn<bool>("destinationEntryIsDirectory");

        QTest::newRow("existing-file") << false;
        QTest::newRow("existing-directory") << true;
    }

    void moveToDestinationWithSameLeafName_failsWithoutOverwriting()
    {
        using namespace MiniCloud::Server;

        QFETCH(bool, destinationEntryIsDirectory);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir storageDirectory(storageRoot);
        QVERIFY(storageDirectory.mkdir(QStringLiteral("Archive")));

        const QDir documentsDirectory(storageDirectory.filePath(QStringLiteral("Documents")));
        const QString sourcePath = documentsDirectory.filePath(QStringLiteral("report.txt"));
        QFile sourceFile(sourcePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write("source-data"), qint64{11});
        sourceFile.close();

        const QDir archiveDirectory(storageDirectory.filePath(QStringLiteral("Archive")));
        const QString destinationPath = archiveDirectory.filePath(QStringLiteral("report.txt"));

        if (destinationEntryIsDirectory)
        {
            QVERIFY(archiveDirectory.mkdir(QStringLiteral("report.txt")));
        }
        else
        {
            QFile destinationFile(destinationPath);
            QVERIFY(destinationFile.open(QIODevice::WriteOnly));
            QCOMPARE(destinationFile.write("target-data"), qint64{11});
            destinationFile.close();
        }

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.move(
            QStringLiteral("/Documents/report.txt"),
            QStringLiteral("/Archive"));

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo(sourcePath).isFile());
        QFile unchangedSourceFile(sourcePath);
        QVERIFY(unchangedSourceFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedSourceFile.readAll(), QByteArrayLiteral("source-data"));

        const QFileInfo destinationInfo(destinationPath);
        QVERIFY(destinationInfo.exists());
        QCOMPARE(destinationInfo.isDir(), destinationEntryIsDirectory);

        if (!destinationEntryIsDirectory)
        {
            QFile unchangedDestinationFile(destinationPath);
            QVERIFY(unchangedDestinationFile.open(QIODevice::ReadOnly));
            QCOMPARE(unchangedDestinationFile.readAll(), QByteArrayLiteral("target-data"));
        }

        QCOMPARE(documentsDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries), QStringList{QStringLiteral("report.txt")});
        QCOMPARE(archiveDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries), QStringList{QStringLiteral("report.txt")});
    }

    void removeExistingFile_deletesAndReturnsLogicalPath()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString reportNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.txt"));
        QFile reportFile(reportNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.remove(QStringLiteral("/Documents/report.txt"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Documents/report.txt"));
        QVERIFY(!QFileInfo::exists(reportNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
    }

    void removeEmptyDirectory_deletesAndReturnsLogicalPath()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/EmptyFolder"))));

        const QString emptyFolderNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/EmptyFolder"));
        const QDir emptyFolderDirectory(emptyFolderNativePath);
        QVERIFY(emptyFolderDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty());

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.remove(QStringLiteral("/Documents/EmptyFolder"));

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.path, QStringLiteral("/Documents/EmptyFolder"));
        QVERIFY(!QFileInfo::exists(emptyFolderNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
    }

    void removeNonEmptyDirectory_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents/Projects"))));

        const QDir storageDirectory(storageRoot);
        const QString projectsNativePath = storageDirectory.filePath(QStringLiteral("Documents/Projects"));
        const QString planNativePath = QDir(projectsNativePath).filePath(QStringLiteral("plan.txt"));
        QFile planFile(planNativePath);
        QVERIFY(planFile.open(QIODevice::WriteOnly));
        QCOMPARE(planFile.write("plan"), qint64{4});
        planFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.remove(QStringLiteral("/Documents/Projects"));

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo(projectsNativePath).isDir());
        QVERIFY(QFileInfo(planNativePath).isFile());
    }

    void removeInvalidOrProtectedPath_failsWithoutMutation_data()
    {
        QTest::addColumn<QString>("logicalPath");

        QTest::newRow("missing-path") << QStringLiteral("/Documents/missing.txt");
        QTest::newRow("root-path") << QStringLiteral("/");
        QTest::newRow("traversal-path") << QStringLiteral("/../outside");
        QTest::newRow("within-root-traversal") << QStringLiteral("/Documents/..");
        QTest::newRow("backslash-path") << QStringLiteral("/Documents\\report.txt");
    }

    void removeInvalidOrProtectedPath_failsWithoutMutation()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, logicalPath);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString outsideRoot = temporaryDirectory.filePath(QStringLiteral("outside"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));
        QVERIFY(QDir().mkpath(outsideRoot));

        const QString reportNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.txt"));
        QFile reportFile(reportNativePath);
        QVERIFY(reportFile.open(QIODevice::WriteOnly));
        QCOMPARE(reportFile.write("report"), qint64{6});
        reportFile.close();

        const QString outsideSecretPath = QDir(outsideRoot).filePath(QStringLiteral("secret.txt"));
        QFile outsideSecretFile(outsideSecretPath);
        QVERIFY(outsideSecretFile.open(QIODevice::WriteOnly));
        QCOMPARE(outsideSecretFile.write("secret"), qint64{6});
        outsideSecretFile.close();

        FileManager manager(storageRoot);

        const FileManagerOperationResult result = manager.remove(logicalPath);

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.path.isEmpty());

        QVERIFY(QFileInfo(reportNativePath).isFile());
        QVERIFY(QFileInfo(outsideSecretPath).isFile());
    }

    void uploadSequentialChunks_commitsFileOnlyAfterExactFinalChunk()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult startResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5);

        QCOMPARE(startResult.status, FileManagerOperationStatus::Success);
        QVERIFY(startResult.errorMessage.isEmpty());
        QCOMPARE(startResult.path, QStringLiteral("/Documents/report.bin"));
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const FileManagerUploadChunkResult firstChunkResult = manager.appendUploadChunk(0, QByteArrayLiteral("abc"));

        QCOMPARE(firstChunkResult.status, FileManagerOperationStatus::Success);
        QVERIFY(firstChunkResult.errorMessage.isEmpty());
        QVERIFY(!firstChunkResult.completed);
        QVERIFY(firstChunkResult.path.isEmpty());
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());

        const FileManagerUploadChunkResult finalChunkResult = manager.appendUploadChunk(3, QByteArrayLiteral("de"));

        QCOMPARE(finalChunkResult.status, FileManagerOperationStatus::Success);
        QVERIFY(finalChunkResult.errorMessage.isEmpty());
        QVERIFY(finalChunkResult.completed);
        QCOMPARE(finalChunkResult.path, QStringLiteral("/Documents/report.bin"));

        QVERIFY(QFileInfo(finalNativePath).isFile());

        QFile finalFile(finalNativePath);
        QVERIFY(finalFile.open(QIODevice::ReadOnly));
        QCOMPARE(finalFile.readAll(), QByteArrayLiteral("abcde"));
    }

    void uploadChunk_withUnexpectedOffset_failsAndAllowsCleanRestart()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult startResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 5);

        QCOMPARE(startResult.status, FileManagerOperationStatus::Success);

        const FileManagerUploadChunkResult invalidChunkResult = manager.appendUploadChunk(1, QByteArrayLiteral("abc"));

        QCOMPARE(invalidChunkResult.status, FileManagerOperationStatus::Failed);
        QVERIFY(!invalidChunkResult.errorMessage.isEmpty());
        QVERIFY(!invalidChunkResult.completed);
        QVERIFY(invalidChunkResult.path.isEmpty());
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());

        const FileManagerUploadStartResult restartResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 2);

        QCOMPARE(restartResult.status, FileManagerOperationStatus::Success);

        const FileManagerUploadChunkResult completedResult = manager.appendUploadChunk(0, QByteArrayLiteral("ok"));

        QCOMPARE(completedResult.status, FileManagerOperationStatus::Success);
        QVERIFY(completedResult.completed);
        QCOMPARE(completedResult.path, QStringLiteral("/Documents/report.bin"));

        QFile finalFile(finalNativePath);
        QVERIFY(finalFile.open(QIODevice::ReadOnly));
        QCOMPARE(finalFile.readAll(), QByteArrayLiteral("ok"));
    }

    void uploadChunk_exceedingDeclaredSize_failsAndAllowsCleanRestart()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/report.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult startResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 3);

        QCOMPARE(startResult.status, FileManagerOperationStatus::Success);

        const FileManagerUploadChunkResult result = manager.appendUploadChunk(0, QByteArrayLiteral("four"));

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(!result.completed);
        QVERIFY(result.path.isEmpty());
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());

        const FileManagerUploadStartResult restartResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 3);

        QCOMPARE(restartResult.status, FileManagerOperationStatus::Success);

        const FileManagerUploadChunkResult completedResult = manager.appendUploadChunk(0, QByteArrayLiteral("abc"));

        QCOMPARE(completedResult.status, FileManagerOperationStatus::Success);
        QVERIFY(completedResult.completed);
        QCOMPARE(completedResult.path, QStringLiteral("/Documents/report.bin"));

        QFile finalFile(finalNativePath);
        QVERIFY(finalFile.open(QIODevice::ReadOnly));
        QCOMPARE(finalFile.readAll(), QByteArrayLiteral("abc"));
    }

    void beginZeroByteUpload_commitsEmptyFileImmediately()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/empty.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult result = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("empty.bin"), 0);

        QCOMPARE(result.status, FileManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(result.completed);
        QCOMPARE(result.path, QStringLiteral("/Documents/empty.bin"));

        QVERIFY(QFileInfo(finalNativePath).isFile());
        QCOMPARE(QFileInfo(finalNativePath).size(), qint64{0});

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(browseResult.entries.size(), 1);
        QCOMPARE(browseResult.entries.constFirst().name, QStringLiteral("empty.bin"));
        QCOMPARE(browseResult.entries.constFirst().path, QStringLiteral("/Documents/empty.bin"));

        const FileManagerUploadChunkResult unexpectedChunkResult = manager.appendUploadChunk(0, QByteArray());

        QCOMPARE(unexpectedChunkResult.status, FileManagerOperationStatus::Failed);
        QVERIFY(!unexpectedChunkResult.errorMessage.isEmpty());
        QVERIFY(!unexpectedChunkResult.completed);
        QVERIFY(unexpectedChunkResult.path.isEmpty());
    }

    void uploadChunk_withoutActiveUpload_failsWithoutCreatingFile()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        FileManager manager(storageRoot);

        const FileManagerUploadChunkResult result = manager.appendUploadChunk(0, QByteArrayLiteral("data"));

        QCOMPARE(result.status, FileManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(!result.completed);
        QVERIFY(result.path.isEmpty());

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());
    }

    void uploadChunk_largerThan64KiB_failsAndAllowsCleanRestart()
    {
        using namespace MiniCloud::Server;

        constexpr qsizetype maximumChunkSize = 64 * 1024;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QString finalNativePath = QDir(storageRoot).filePath(QStringLiteral("Documents/large.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult startResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("large.bin"), maximumChunkSize + 1);

        QCOMPARE(startResult.status, FileManagerOperationStatus::Success);

        const FileManagerUploadChunkResult oversizedChunkResult = manager.appendUploadChunk(0, QByteArray(maximumChunkSize + 1, 'x'));

        QCOMPARE(oversizedChunkResult.status, FileManagerOperationStatus::Failed);
        QVERIFY(!oversizedChunkResult.errorMessage.isEmpty());
        QVERIFY(!oversizedChunkResult.completed);
        QVERIFY(oversizedChunkResult.path.isEmpty());
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());

        const FileManagerUploadStartResult restartResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("large.bin"), maximumChunkSize + 1);
        QCOMPARE(restartResult.status, FileManagerOperationStatus::Success);

        const QByteArray firstChunk(maximumChunkSize, 'a');
        const QByteArray finalChunk(1, 'b');
        const FileManagerUploadChunkResult firstChunkResult = manager.appendUploadChunk(0, firstChunk);

        QCOMPARE(firstChunkResult.status, FileManagerOperationStatus::Success);
        QVERIFY(!firstChunkResult.completed);

        const FileManagerUploadChunkResult completedResult = manager.appendUploadChunk(static_cast<quint64>(firstChunk.size()), finalChunk);

        QCOMPARE(completedResult.status, FileManagerOperationStatus::Success);
        QVERIFY(completedResult.completed);
        QCOMPARE(completedResult.path, QStringLiteral("/Documents/large.bin"));

        QFile finalFile(finalNativePath);
        QVERIFY(finalFile.open(QIODevice::ReadOnly));
        QCOMPARE(finalFile.readAll(), firstChunk + finalChunk);
    }

    void beginUpload_existingFileOrDirectory_failsWithoutOverwriting_data()
    {
        QTest::addColumn<bool>("targetIsDirectory");

        QTest::newRow("existing-file") << false;
        QTest::newRow("existing-directory") << true;
    }

    void beginUpload_existingFileOrDirectory_failsWithoutOverwriting()
    {
        using namespace MiniCloud::Server;

        QFETCH(bool, targetIsDirectory);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        const QString existingEntryPath = documentsDirectory.filePath(QStringLiteral("report.bin"));

        if (targetIsDirectory)
        {
            QVERIFY(documentsDirectory.mkdir(QStringLiteral("report.bin")));
        }
        else
        {
            QFile existingFile(existingEntryPath);
            QVERIFY(existingFile.open(QIODevice::WriteOnly));
            QCOMPARE(existingFile.write("old-data"), qint64{8});
            existingFile.close();
        }

        FileManager manager(storageRoot);

        const FileManagerUploadStartResult collisionResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 3);

        QCOMPARE(collisionResult.status, FileManagerOperationStatus::Failed);
        QVERIFY(!collisionResult.errorMessage.isEmpty());
        QVERIFY(!collisionResult.completed);
        QVERIFY(collisionResult.path.isEmpty());

        const QFileInfo existingEntryInfo(existingEntryPath);
        QVERIFY(existingEntryInfo.exists());
        QCOMPARE(existingEntryInfo.isDir(), targetIsDirectory);

        if (!targetIsDirectory)
        {
            QFile unchangedFile(existingEntryPath);
            QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
            QCOMPARE(unchangedFile.readAll(), QByteArrayLiteral("old-data"));
        }

        const FileManagerUploadStartResult newNameResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("new-report.bin"), 3);

        QCOMPARE(newNameResult.status, FileManagerOperationStatus::Success);
        QVERIFY(newNameResult.errorMessage.isEmpty());
        QVERIFY(!newNameResult.completed);
        QCOMPARE(newNameResult.path, QStringLiteral("/Documents/new-report.bin"));
    }

    void beginUpload_invalidDestinationOrFileName_failsWithoutCreatingTemporaryFile_data()
    {
        QTest::addColumn<QString>("destinationPath");
        QTest::addColumn<QString>("fileName");

        QTest::newRow("empty-destination") << QString() << QStringLiteral("report.bin");
        QTest::newRow("traversal-destination") << QStringLiteral("/../outside") << QStringLiteral("report.bin");
        QTest::newRow("missing-destination") << QStringLiteral("/Missing") << QStringLiteral("report.bin");
        QTest::newRow("file-destination") << QStringLiteral("/Documents/target.txt") << QStringLiteral("report.bin");
        QTest::newRow("empty-file-name") << QStringLiteral("/Documents") << QString();
        QTest::newRow("whitespace-file-name") << QStringLiteral("/Documents") << QStringLiteral("   ");
        QTest::newRow("dot-file-name") << QStringLiteral("/Documents") << QStringLiteral(".");
        QTest::newRow("dotdot-file-name") << QStringLiteral("/Documents") << QStringLiteral("..");
        QTest::newRow("slash-file-name") << QStringLiteral("/Documents") << QStringLiteral("nested/report.bin");
        QTest::newRow("backslash-file-name") << QStringLiteral("/Documents") << QStringLiteral("nested\\report.bin");
    }

    void beginUpload_invalidDestinationOrFileName_failsWithoutCreatingTemporaryFile()
    {
        using namespace MiniCloud::Server;

        QFETCH(QString, destinationPath);
        QFETCH(QString, fileName);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        const QString targetFilePath = documentsDirectory.filePath(QStringLiteral("target.txt"));
        QFile targetFile(targetFilePath);
        QVERIFY(targetFile.open(QIODevice::WriteOnly));
        QCOMPARE(targetFile.write("unchanged"), qint64{9});
        targetFile.close();

        const QString finalNativePath = documentsDirectory.filePath(QStringLiteral("report.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult invalidResult = manager.beginUpload(destinationPath, fileName, 3);

        QCOMPARE(invalidResult.status, FileManagerOperationStatus::Failed);
        QVERIFY(!invalidResult.errorMessage.isEmpty());
        QVERIFY(!invalidResult.completed);
        QVERIFY(invalidResult.path.isEmpty());
        QVERIFY(!QFileInfo::exists(finalNativePath));

        const FileManagerBrowseResult rootBrowseResult = manager.browse(QStringLiteral("/"));
        QCOMPARE(rootBrowseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(rootBrowseResult.entries.size(), 1);
        QCOMPARE(rootBrowseResult.entries.constFirst().name, QStringLiteral("Documents"));

        const FileManagerBrowseResult documentsBrowseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(documentsBrowseResult.status, FileManagerOperationStatus::Success);
        QCOMPARE(documentsBrowseResult.entries.size(), 1);
        QCOMPARE(documentsBrowseResult.entries.constFirst().name, QStringLiteral("target.txt"));
        QCOMPARE(documentsBrowseResult.entries.constFirst().path, QStringLiteral("/Documents/target.txt"));

        QFile unchangedTargetFile(targetFilePath);
        QVERIFY(unchangedTargetFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedTargetFile.readAll(), QByteArrayLiteral("unchanged"));

        const FileManagerUploadStartResult validResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("report.bin"), 3);

        QCOMPARE(validResult.status, FileManagerOperationStatus::Success);
        QVERIFY(validResult.errorMessage.isEmpty());
        QVERIFY(!validResult.completed);
        QCOMPARE(validResult.path, QStringLiteral("/Documents/report.bin"));
    }

    void beginUpload_whileAnotherUploadIsActive_failsWithoutDisturbingOriginalTransfer()
    {
        using namespace MiniCloud::Server;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(QDir(storageRoot).filePath(QStringLiteral("Documents"))));

        const QDir documentsDirectory(QDir(storageRoot).filePath(QStringLiteral("Documents")));
        const QString firstNativePath = documentsDirectory.filePath(QStringLiteral("first.bin"));
        const QString secondNativePath = documentsDirectory.filePath(QStringLiteral("second.bin"));
        FileManager manager(storageRoot);

        const FileManagerUploadStartResult firstStartResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("first.bin"), 4);

        QCOMPARE(firstStartResult.status, FileManagerOperationStatus::Success);
        QVERIFY(!firstStartResult.completed);

        const FileManagerUploadStartResult secondStartResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("second.bin"), 3);

        QCOMPARE(secondStartResult.status, FileManagerOperationStatus::Failed);
        QVERIFY(!secondStartResult.errorMessage.isEmpty());
        QVERIFY(!secondStartResult.completed);
        QVERIFY(secondStartResult.path.isEmpty());
        QVERIFY(!QFileInfo::exists(secondNativePath));

        const FileManagerBrowseResult browseResult = manager.browse(QStringLiteral("/Documents"));
        QCOMPARE(browseResult.status, FileManagerOperationStatus::Success);
        QVERIFY(browseResult.entries.isEmpty());

        const FileManagerUploadChunkResult firstCompletedResult = manager.appendUploadChunk(0, QByteArrayLiteral("data"));

        QCOMPARE(firstCompletedResult.status, FileManagerOperationStatus::Success);
        QVERIFY(firstCompletedResult.completed);
        QCOMPARE(firstCompletedResult.path, QStringLiteral("/Documents/first.bin"));

        QFile firstFile(firstNativePath);
        QVERIFY(firstFile.open(QIODevice::ReadOnly));
        QCOMPARE(firstFile.readAll(), QByteArrayLiteral("data"));

        const FileManagerUploadStartResult secondRetryResult = manager.beginUpload(QStringLiteral("/Documents"), QStringLiteral("second.bin"), 3);

        QCOMPARE(secondRetryResult.status, FileManagerOperationStatus::Success);
        QVERIFY(secondRetryResult.errorMessage.isEmpty());
        QVERIFY(!secondRetryResult.completed);
        QCOMPARE(secondRetryResult.path, QStringLiteral("/Documents/second.bin"));
    }
};

QTEST_MAIN(FileManagerTest)
#include "filemanagertest.moc"
