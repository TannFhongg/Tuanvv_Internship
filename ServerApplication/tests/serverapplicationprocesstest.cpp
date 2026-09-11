#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include "applicationcontroller.h"
#include "licensemanager.h"

namespace
{
    class ProcessCleanup
    {
    public:
        explicit ProcessCleanup(QProcess &process) : m_process(process) {}

        ~ProcessCleanup()
        {
            stop();
        }

        void stop()
        {
            if (m_process.state() == QProcess::NotRunning)
            {
                return;
            }

            m_process.terminate();
            if (!m_process.waitForFinished(3000))
            {
                m_process.kill();
                m_process.waitForFinished(3000);
            }
        }

    private:
        QProcess &m_process;
    };
}

class ServerApplicationProcessTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<MiniCloud::Client::ClientAccessState>();
        qRegisterMetaType<MiniCloud::Protocol::AuthenticationStatus>();
        qRegisterMetaType<MiniCloud::Protocol::ErrorResponseData>();
        qRegisterMetaType<MiniCloud::Client::RequestDispatchError>();
        qRegisterMetaType<MiniCloud::Protocol::FileEntryData>();
        qRegisterMetaType<QList<MiniCloud::Protocol::FileEntryData>>();
    }

    void launch_withConfiguredDataDirectory_servesActivatedClientOverTcp()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Client::RequestSendStatus;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseStoragePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QString fileStorageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(fileStorageRoot));

        QFile seedFile(QDir(fileStorageRoot).filePath(QStringLiteral("seed.txt")));
        QVERIFY(seedFile.open(QIODevice::WriteOnly));
        QCOMPARE(seedFile.write("seed file content"), qint64{17});
        seedFile.close();

        LicenseManager licenseManager(licenseStoragePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);
        const auto createLicenseResult = licenseManager.createLicense();
        QCOMPARE(createLicenseResult.operationStatus, LicenseManagerOperationStatus::Success);

        QTcpServer portReservation;
        QVERIFY(portReservation.listen(QHostAddress::LocalHost, 0));
        const quint16 port = portReservation.serverPort();
        QVERIFY(port != 0);
        portReservation.close();

        const QString serverApplicationPath = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("ServerApplication.exe"));
        QVERIFY2(QFileInfo::exists(serverApplicationPath), qPrintable(serverApplicationPath));

        QProcess serverProcess;
        ProcessCleanup processCleanup(serverProcess);
        serverProcess.setProgram(serverApplicationPath);
        serverProcess.setArguments(
            {
                QStringLiteral("--data-dir"),
                temporaryDirectory.path(),
                QStringLiteral("--listen-address"),
                QStringLiteral("127.0.0.1"),
                QStringLiteral("--listen-port"),
                QString::number(port),
                QStringLiteral("-platform"),
                QStringLiteral("offscreen")
            });
        serverProcess.start();
        QVERIFY2(serverProcess.waitForStarted(3000), qPrintable(serverProcess.errorString()));

        ApplicationController applicationController;
        QSignalSpy connectionStateChangedSpy(
            &applicationController,
            &ApplicationController::connectionStateChanged);
        QSignalSpy activationRejectedSpy(
            &applicationController,
            &ApplicationController::activationRejected);
        QSignalSpy activationErrorSpy(
            &applicationController,
            &ApplicationController::activationError);
        QSignalSpy activationFailedSpy(
            &applicationController,
            &ApplicationController::activationFailed);
        QSignalSpy browseReceivedSpy(
            &applicationController,
            &ApplicationController::browseReceived);
        QSignalSpy browseFailedSpy(
            &applicationController,
            &ApplicationController::browseFailed);
        QVERIFY(connectionStateChangedSpy.isValid());
        QVERIFY(activationRejectedSpy.isValid());
        QVERIFY(activationErrorSpy.isValid());
        QVERIFY(activationFailedSpy.isValid());
        QVERIFY(browseReceivedSpy.isValid());
        QVERIFY(browseFailedSpy.isValid());

        QTRY_VERIFY_WITH_TIMEOUT(
            ([&applicationController, port, &serverProcess]()
             {
                 if (serverProcess.state() != QProcess::Running)
                 {
                     return false;
                 }

                 if (!applicationController.isConnected())
                {
                    applicationController.connectToServer(QStringLiteral("127.0.0.1"), port);
                }

                 return applicationController.isConnected();
             }()),
            5000);

        const auto activationResult = applicationController.activate(
            createLicenseResult.productKey,
            QStringLiteral("DEVICE-PROCESS-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);
        QTRY_COMPARE(applicationController.accessState(), ClientAccessState::Active);
        QCOMPARE(activationRejectedSpy.count(), 0);
        QCOMPARE(activationErrorSpy.count(), 0);
        QCOMPARE(activationFailedSpy.count(), 0);

        QVERIFY(applicationController.requestBrowse(QStringLiteral("/")));
        QTRY_COMPARE(browseReceivedSpy.count(), 1);

        const QList<FileEntryData> entries =
            qvariant_cast<QList<FileEntryData>>(browseReceivedSpy.takeFirst().at(1));
        const auto seedEntry = std::find_if(
            entries.cbegin(),
            entries.cend(),
            [](const FileEntryData &entry)
            {
                return entry.name == QStringLiteral("seed.txt");
            });
        QVERIFY(seedEntry != entries.cend());
        QCOMPARE(seedEntry->type, FileEntryType::File);
        QCOMPARE(browseFailedSpy.count(), 0);
        QVERIFY(serverProcess.state() == QProcess::Running);

        applicationController.disconnectFromServer();
        QTRY_VERIFY(!applicationController.isConnected());
        processCleanup.stop();
        QCOMPARE(serverProcess.state(), QProcess::NotRunning);
    }
};

QTEST_GUILESS_MAIN(ServerApplicationProcessTest)

#include "serverapplicationprocesstest.moc"
