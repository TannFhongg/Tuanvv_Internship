#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "licenserepository.h"
#include "licensemanager.h"
#include <QObject>

using MiniCloud::Protocol::AuthenticationStatus;
using MiniCloud::Server::AuthenticationResult;
using MiniCloud::Server::LicenseManager;
using MiniCloud::Server::LicenseManagerOperationStatus;
using MiniCloud::Server::LicenseManagerResult;
using MiniCloud::Server::LicenseRecord;
using MiniCloud::Server::LicenseRepository;
using MiniCloud::Server::LicenseRepositoryResult;
using MiniCloud::Server::LicenseRepositoryStatus;
class LicenseManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initialize_missingRepository_succeeds()
    {

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult result = manager.initialize();

        QCOMPARE(result.status, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void initialize_malformedRepository_failsAndRemainsUninitialized()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        const QByteArray malformedData = R"({"schemaVersion":1,"licenses":[)";
        QCOMPARE(file.write(malformedData), malformedData.size());
        file.close();

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult result = manager.initialize();
        QCOMPARE(result.status, LicenseManagerOperationStatus::Failed);
        QVERIFY(QFileInfo::exists(filePath));
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(!manager.isInitialized());
    }

    void authenticate_beforeInitialization_fails()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        QVERIFY(!manager.isInitialized());

        AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-TEST-0001"),
            QStringLiteral("DEVICE-001"));

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Failed);

        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Invalid);

        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(!manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void authenticate_blankRequiredField_fails_data()
    {
        QTest::addColumn<QString>("productKey");
        QTest::addColumn<QString>("deviceId");

        QTest::newRow("empty-product-key") << QStringLiteral("") << QStringLiteral("DEVICE-001");
        QTest::newRow("blank-product-key") << QStringLiteral("   ") << QStringLiteral("DEVICE-001");
        QTest::newRow("empty-device-id") << QStringLiteral("MCLD-TEST-0001") << QStringLiteral("");
        QTest::newRow("blank-device-id") << QStringLiteral("MCLD-TEST-0001") << QStringLiteral("   ");
    }

    void authenticate_blankRequiredField_fails()
    {
        QFETCH(QString, productKey);
        QFETCH(QString, deviceId);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        QVERIFY(!manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(productKey, deviceId);

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Failed);

        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Invalid);

        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(!manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void authenticate_unknownProductKey_returnsInvalidKey()
    {

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-TEST-0001"),
            QStringLiteral("DEVICE-001"));

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(result.authenticationStatus, AuthenticationStatus::InvalidKey);
        QVERIFY(result.errorMessage.isEmpty());
    }

    void authenticate_disabledLicense_returnsDisabledBeforeDeviceCheck()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);
        const LicenseRecord disabledRecord{
            QStringLiteral("MCLD-TEST-0001"),
            QStringLiteral("DEVICE-001"),
            false};

        const LicenseRepositoryResult insertResult = repository.insert(disabledRecord);

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-TEST-0001"),
            QStringLiteral("DEVICE-OTHER"));

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Disabled);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());
        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-TEST-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-TEST-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(reloadedRecord->enabled, false);
    }

    void authenticate_unboundLicense_bindsDeviceAndReturnsValid()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        QVERIFY(!QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);
        const LicenseRecord unboundRecord{
            QStringLiteral("MCLD-TEST-0001"),
            QString(),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QVERIFY(insertResult.status == LicenseRepositoryStatus::Success);
        QVERIFY(insertResult.errorMessage.isEmpty());

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-TEST-0001"),
            QStringLiteral("DEVICE-001"));

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Valid);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        std::optional<LicenseRecord> reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-TEST-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-TEST-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(reloadedRecord->enabled, true);
    }

    void authenticate_boundLicenseWithSameDevice_returnsValid()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        QVERIFY(!QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);
        const LicenseRecord unboundRecord{
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OWNER"),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QVERIFY(insertResult.status == LicenseRepositoryStatus::Success);

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OWNER"));
        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Valid);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        std::optional<LicenseRecord> reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-BOUND-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-BOUND-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-OWNER"));
        QCOMPARE(reloadedRecord->enabled, true);
    }

    void authenticate_boundLicenseWithDifferentDevice_returnsDeviceMismatch()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        QVERIFY(!QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);
        const LicenseRecord unboundRecord{
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OWNER"),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QVERIFY(insertResult.status == LicenseRepositoryStatus::Success);

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OTHER"));

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(result.authenticationStatus, AuthenticationStatus::DeviceMismatch);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const LicenseRepositoryResult reloadResult = repository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        std::optional<LicenseRecord> reloadedRecord = repository.findByProductKey(QStringLiteral("MCLD-BOUND-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-BOUND-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-OWNER"));
        QCOMPARE(reloadedRecord->enabled, true);
    }

    void authenticate_unboundLicensePersistenceFails_returnsOperationFailure()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString storagePath = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString movedStoragePath = temporaryDirectory.filePath(QStringLiteral("storage-moved"));
        const QString filePath = QDir(storagePath).filePath(QStringLiteral("licenses.json"));
        const QString movedFilePath = QDir(movedStoragePath).filePath(QStringLiteral("licenses.json"));

        QDir temporaryDirectoryPath(temporaryDirectory.path());
        QVERIFY(temporaryDirectoryPath.mkpath(QStringLiteral("storage")));

        LicenseRepository repository(filePath);
        const LicenseRecord unboundRecord{
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral(""),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(unboundRecord);
        QVERIFY(insertResult.status == LicenseRepositoryStatus::Success);

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));

        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        const AuthenticationResult result = manager.authenticate(
            QStringLiteral("MCLD-BOUND-0001"),
            QStringLiteral("DEVICE-OTHER"));

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Failed);
        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Invalid);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        LicenseRepository reloadedRepository(movedFilePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        const std::optional<LicenseRecord> reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-BOUND-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-BOUND-0001"));
        QCOMPARE(reloadedRecord->deviceId, QString());
        QCOMPARE(reloadedRecord->enabled, true);
    }

    void createLicense_beforeInitialization_fails()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        QVERIFY(!manager.isInitialized());

        const auto result = manager.createLicense();

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Failed);
        QVERIFY(result.productKey.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }

    void createLicense_afterInitialization_persistsEnabledUnboundLicense()
    {

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const auto result = manager.createLicense();

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QVERIFY(!result.productKey.isEmpty());
        QVERIFY(result.errorMessage.isEmpty());

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());
    }

    void createLicense_twice_generatesDistinctPersistedKeys()
    {

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const auto result1 = manager.createLicense();
        QCOMPARE(result1.operationStatus, LicenseManagerOperationStatus::Success);
        QVERIFY(!result1.productKey.isEmpty());
        QVERIFY(result1.errorMessage.isEmpty());

        const auto result2 = manager.createLicense();
        QCOMPARE(result2.operationStatus, LicenseManagerOperationStatus::Success);
        QVERIFY(!result2.productKey.isEmpty());
        QVERIFY(result2.errorMessage.isEmpty());

        QVERIFY(result1.productKey != result2.productKey);

        LicenseRepository loadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = loadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        std::optional<LicenseRecord> reloadedRecord1 = loadedRepository.findByProductKey(result1.productKey);
        QVERIFY(reloadedRecord1.has_value());
        QCOMPARE(reloadedRecord1->productKey, result1.productKey);
        QCOMPARE(reloadedRecord1->deviceId, QString());
        QVERIFY(reloadedRecord1->enabled);

        std::optional<LicenseRecord> reloadedRecord2 = loadedRepository.findByProductKey(result2.productKey);
        QVERIFY(reloadedRecord2.has_value());
        QCOMPARE(reloadedRecord2->productKey, result2.productKey);
        QCOMPARE(reloadedRecord2->deviceId, QString());
        QVERIFY(reloadedRecord2->enabled);
    }

    void createLicense_persistenceFails_returnsFailureWithoutProductKey()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString missingParentPath = temporaryDirectory.filePath(QStringLiteral("missing-parent"));

        const QString filePath = QDir(missingParentPath).filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QDir(missingParentPath).exists());
        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const auto result = manager.createLicense();

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Failed);
        QVERIFY(result.productKey.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
    }
};

QTEST_MAIN(LicenseManagerTest)
#include "licensemanagertest.moc"
