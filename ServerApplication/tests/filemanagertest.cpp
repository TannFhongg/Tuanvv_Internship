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
};

QTEST_MAIN(FileManagerTest)
#include "filemanagertest.moc"
