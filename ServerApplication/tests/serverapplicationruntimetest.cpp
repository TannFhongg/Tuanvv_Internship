#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTcpSocket>
#include <QtTest>

#include "applicationcontroller.h"
#include "licensemanager.h"
#include "protocolconstants.h"
#include "serverapplicationruntime.h"

class ServerApplicationRuntimeTest : public QObject
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

    void activatedClient_browseRoot_overRuntimeTcp_receivesSeededEntry()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Client::RequestSendStatus;
        using MiniCloud::Protocol::FileEntryData;
        using MiniCloud::Protocol::FileEntryType;
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        ServerApplicationRuntime::Configuration configuration;
        configuration.address = QHostAddress::LocalHost;
        configuration.port = 0;
        configuration.licenseStoragePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        configuration.fileStorageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));
        QVERIFY(QDir().mkpath(configuration.fileStorageRoot));

        QFile seedFile(QDir(configuration.fileStorageRoot).filePath(QStringLiteral("seed.txt")));
        QVERIFY(seedFile.open(QIODevice::WriteOnly));
        QCOMPARE(seedFile.write("seed file content"), qint64{17});
        seedFile.close();

        LicenseManager licenseManager(configuration.licenseStoragePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);
        const auto createLicenseResult = licenseManager.createLicense();
        QCOMPARE(createLicenseResult.operationStatus, LicenseManagerOperationStatus::Success);

        ServerApplicationRuntime runtime(configuration);
        QVERIFY2(runtime.start(), qPrintable(runtime.lastError()));
        QVERIFY(runtime.boundPort() != 0);

        ApplicationController applicationController;
        QSignalSpy connectionStateChangedSpy(
            &applicationController, &ApplicationController::connectionStateChanged);
        QSignalSpy connectionFailedSpy(
            &applicationController, &ApplicationController::connectionFailed);
        QSignalSpy accessStateChangedSpy(
            &applicationController, &ApplicationController::accessStateChanged);
        QSignalSpy activationRejectedSpy(
            &applicationController, &ApplicationController::activationRejected);
        QSignalSpy activationErrorSpy(
            &applicationController, &ApplicationController::activationError);
        QSignalSpy activationFailedSpy(
            &applicationController, &ApplicationController::activationFailed);
        QSignalSpy browseReceivedSpy(
            &applicationController, &ApplicationController::browseReceived);
        QSignalSpy browseFailedSpy(
            &applicationController, &ApplicationController::browseFailed);
        QSignalSpy fileOperationFailedSpy(
            &applicationController, &ApplicationController::fileOperationFailed);

        QVERIFY(connectionStateChangedSpy.isValid());
        QVERIFY(connectionFailedSpy.isValid());
        QVERIFY(accessStateChangedSpy.isValid());
        QVERIFY(activationRejectedSpy.isValid());
        QVERIFY(activationErrorSpy.isValid());
        QVERIFY(activationFailedSpy.isValid());
        QVERIFY(browseReceivedSpy.isValid());
        QVERIFY(browseFailedSpy.isValid());
        QVERIFY(fileOperationFailedSpy.isValid());

        QVERIFY(applicationController.connectToServer(QStringLiteral("127.0.0.1"), runtime.boundPort()));
        QTRY_VERIFY(applicationController.isConnected());
        QTRY_VERIFY(connectionStateChangedSpy.count() > 0);

        const auto activationResult = applicationController.activate(
            createLicenseResult.productKey,
            QStringLiteral("DEVICE-RUNTIME-CLIENT"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);
        QTRY_COMPARE(applicationController.accessState(), ClientAccessState::Active);
        QTRY_VERIFY(accessStateChangedSpy.count() >= 2);
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
        QCOMPARE(fileOperationFailedSpy.count(), 0);
        QCOMPARE(connectionFailedSpy.count(), 0);
        QCOMPARE(connectionStateChangedSpy.count(), 1);
        QCOMPARE(connectionStateChangedSpy.at(0).at(0).toBool(), true);

        applicationController.disconnectFromServer();
        QTRY_VERIFY(!applicationController.isConnected());
        runtime.stop();
    }

    void activatedClient_uploadThenDownload_overRuntimeTcp_roundTripsMultiChunkContent()
    {
        using MiniCloud::Client::ClientAccessState;
        using MiniCloud::Client::RequestSendStatus;
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir serverTemporaryDirectory;
        QTemporaryDir clientTemporaryDirectory;
        QVERIFY(serverTemporaryDirectory.isValid());
        QVERIFY(clientTemporaryDirectory.isValid());

        ServerApplicationRuntime::Configuration configuration;
        configuration.address = QHostAddress::LocalHost;
        configuration.port = 0;
        configuration.licenseStoragePath = serverTemporaryDirectory.filePath(QStringLiteral("licenses.json"));
        configuration.fileStorageRoot = serverTemporaryDirectory.filePath(QStringLiteral("storage"));

        LicenseManager licenseManager(configuration.licenseStoragePath);
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);
        const auto createLicenseResult = licenseManager.createLicense();
        QCOMPARE(createLicenseResult.operationStatus, LicenseManagerOperationStatus::Success);

        const qsizetype contentSize = static_cast<qsizetype>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes) + 17;
        QByteArray sourceContent(contentSize, '\0');
        for (qsizetype index = 0; index < sourceContent.size(); ++index)
        {
            sourceContent[index] = static_cast<char>(index % 251);
        }

        const QString sourceFilePath = clientTemporaryDirectory.filePath(QStringLiteral("source.bin"));
        const QString targetFilePath = clientTemporaryDirectory.filePath(QStringLiteral("downloaded.bin"));
        QFile sourceFile(sourceFilePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write(sourceContent), static_cast<qint64>(sourceContent.size()));
        sourceFile.close();
        QVERIFY(!QFileInfo::exists(targetFilePath));

        ServerApplicationRuntime runtime(configuration);
        QVERIFY2(runtime.start(), qPrintable(runtime.lastError()));
        QVERIFY(runtime.boundPort() != 0);

        ApplicationController applicationController;
        QSignalSpy connectionStateChangedSpy(
            &applicationController, &ApplicationController::connectionStateChanged);
        QSignalSpy connectionFailedSpy(
            &applicationController, &ApplicationController::connectionFailed);
        QSignalSpy activationRejectedSpy(
            &applicationController, &ApplicationController::activationRejected);
        QSignalSpy activationErrorSpy(
            &applicationController, &ApplicationController::activationError);
        QSignalSpy activationFailedSpy(
            &applicationController, &ApplicationController::activationFailed);
        QSignalSpy completionSpy(
            &applicationController, &ApplicationController::fileOperationCompleted);
        QSignalSpy fileOperationFailedSpy(
            &applicationController, &ApplicationController::fileOperationFailed);
        QVERIFY(connectionStateChangedSpy.isValid());
        QVERIFY(connectionFailedSpy.isValid());
        QVERIFY(activationRejectedSpy.isValid());
        QVERIFY(activationErrorSpy.isValid());
        QVERIFY(activationFailedSpy.isValid());
        QVERIFY(completionSpy.isValid());
        QVERIFY(fileOperationFailedSpy.isValid());

        QVERIFY(applicationController.connectToServer(QStringLiteral("127.0.0.1"), runtime.boundPort()));
        QTRY_VERIFY(applicationController.isConnected());

        const auto activationResult = applicationController.activate(
            createLicenseResult.productKey, QStringLiteral("DEVICE-RUNTIME-TRANSFER"));
        QCOMPARE(activationResult.status, RequestSendStatus::Accepted);
        QTRY_COMPARE(applicationController.accessState(), ClientAccessState::Active);
        QCOMPARE(activationRejectedSpy.count(), 0);
        QCOMPARE(activationErrorSpy.count(), 0);
        QCOMPARE(activationFailedSpy.count(), 0);

        QVERIFY(applicationController.requestUpload(sourceFilePath, QStringLiteral("/")));
        QTRY_COMPARE(completionSpy.count(), 1);
        QCOMPARE(completionSpy.at(0).at(0).toString(), QStringLiteral("/source.bin"));

        QFile serverFile(QDir(configuration.fileStorageRoot).filePath(QStringLiteral("source.bin")));
        QVERIFY(serverFile.open(QIODevice::ReadOnly));
        QCOMPARE(serverFile.readAll(), sourceContent);
        serverFile.close();

        QVERIFY(applicationController.requestDownload(QStringLiteral("/source.bin"), targetFilePath));
        QTRY_COMPARE(completionSpy.count(), 2);
        QCOMPARE(completionSpy.at(1).at(0).toString(), QStringLiteral("/source.bin"));

        QFile targetFile(targetFilePath);
        QVERIFY(targetFile.open(QIODevice::ReadOnly));
        QCOMPARE(targetFile.readAll(), sourceContent);

        QCOMPARE(fileOperationFailedSpy.count(), 0);
        QCOMPARE(connectionFailedSpy.count(), 0);
        QCOMPARE(connectionStateChangedSpy.count(), 1);
        QCOMPARE(connectionStateChangedSpy.at(0).at(0).toBool(), true);

        applicationController.disconnectFromServer();
        QTRY_VERIFY(!applicationController.isConnected());
        runtime.stop();
    }

    void start_withValidConfiguration_listensOnEphemeralPortAndAcceptsTcpConnection()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        ServerApplicationRuntime::Configuration configuration;
        configuration.address = QHostAddress::LocalHost;
        configuration.port = 0;
        configuration.licenseStoragePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        configuration.fileStorageRoot = temporaryDirectory.filePath(QStringLiteral("storage"));

        ServerApplicationRuntime runtime(configuration);

        QVERIFY2(runtime.start(), qPrintable(runtime.lastError()));
        QVERIFY(runtime.isListening());

        const quint16 port = runtime.boundPort();
        QVERIFY(port != 0);

        QTcpSocket clientSocket;
        clientSocket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY2(clientSocket.waitForConnected(3000), qPrintable(clientSocket.errorString()));

        runtime.stop();

        QVERIFY(!runtime.isListening());
    }
};

QTEST_MAIN(ServerApplicationRuntimeTest)

#include "serverapplicationruntimetest.moc"
