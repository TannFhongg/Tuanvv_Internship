#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <array>
#include "licenserepository.h"
#include "licensemanager.h"
#include <QObject>

using MiniCloud::Protocol::AuthenticationStatus;
using MiniCloud::Server::AuthenticationResult;
using MiniCloud::Server::LicenseListResult;
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

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const AuthenticationResult result = manager.authenticate(productKey, deviceId);

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Failed);

        QCOMPARE(result.authenticationStatus, AuthenticationStatus::Invalid);

        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());
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

        const auto reloadedRecord = reloadedRepository.findByProductKey(result.productKey);
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, result.productKey);
        QCOMPARE(reloadedRecord->deviceId, QString());
        QVERIFY(reloadedRecord->enabled);
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

    void disableLicense_existingEnabledLicense_persistsDisabledState()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord originalRecord{
            QStringLiteral("MCLD-DISABLE-0001"),
            QStringLiteral("DEVICE-001"),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(originalRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));

        LicenseManager licenseManager(filePath);

        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(initializeResult.errorMessage, QString());

        const LicenseManagerResult disableResult = licenseManager.disableLicense(QStringLiteral("MCLD-DISABLE-0001"));

        QCOMPARE(disableResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(disableResult.errorMessage, QString());

        const LicenseRepositoryResult repositoryReloadResult = repository.load();
        QCOMPARE(repositoryReloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repositoryReloadResult.errorMessage, QString());

        const auto currentRecord = repository.findByProductKey(QStringLiteral("MCLD-DISABLE-0001"));
        QVERIFY(currentRecord.has_value());
        QCOMPARE(currentRecord->enabled, false);

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        QCOMPARE(reloadedRepository.count(), qsizetype(1));

        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-DISABLE-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-DISABLE-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(reloadedRecord->enabled, false);
    }

    void disableLicense_beforeInitialization_fails()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseManager licenseManager(filePath);

        const LicenseManagerResult disableResult = licenseManager.disableLicense(QStringLiteral("MCLD-DISABLE-0001"));

        QCOMPARE(disableResult.status, LicenseManagerOperationStatus::Failed);
        QVERIFY(!disableResult.errorMessage.isEmpty());
    }

    void disableLicense_blankProductKey_fails_data()
    {
        QTest::addColumn<QString>("productKey");

        QTest::newRow("empty-product-key")
            << QString();

        QTest::newRow("whitespace-product-key")
            << QStringLiteral("   ");
    }

    void disableLicense_blankProductKey_fails()
    {
        QFETCH(QString, productKey);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const LicenseManagerResult result = manager.disableLicense(productKey);

        QCOMPARE(result.status, LicenseManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void disableLicense_unknownProductKey_failsWithoutMutation()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const LicenseManagerResult result = manager.disableLicense(QStringLiteral("MCLD-UNKNOWN-0001"));

        QCOMPARE(result.status, LicenseManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void disableLicense_alreadyDisabledLicense_succeedsWithoutRewrite()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storagePath = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString movedStoragePath = temporaryDirectory.filePath(QStringLiteral("storage-moved"));
        const QString filePath = QDir(storagePath).filePath(QStringLiteral("licenses.json"));
        const QString movedFilePath = QDir(movedStoragePath).filePath(QStringLiteral("licenses.json"));

        QDir temporaryDirectoryPath(temporaryDirectory.path());
        QVERIFY(temporaryDirectoryPath.mkpath(QStringLiteral("storage")));

        LicenseRecord licenseRecord{
            QStringLiteral("MCLD-DISABLE-0001"),
            QStringLiteral("DEVICE-001"),
            false};

        LicenseRepository repository(filePath);
        const LicenseRepositoryResult insertResult = repository.insert(licenseRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertResult.errorMessage, QString());

        LicenseManager licenseManager(filePath);

        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(initializeResult.errorMessage, QString());
        QVERIFY(licenseManager.isInitialized());

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        const LicenseManagerResult result = licenseManager.disableLicense(QStringLiteral("MCLD-DISABLE-0001"));

        QCOMPARE(result.status, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(licenseManager.isInitialized());
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        LicenseRepository reloadedRepository(movedFilePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());

        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-DISABLE-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-DISABLE-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(reloadedRecord->enabled, false);
    }

    void disableLicense_persistenceFails_returnsFailureWithoutChangingState()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storagePath = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString movedStoragePath = temporaryDirectory.filePath(QStringLiteral("storage-moved"));
        const QString filePath = QDir(storagePath).filePath(QStringLiteral("licenses.json"));
        const QString movedFilePath = QDir(movedStoragePath).filePath(QStringLiteral("licenses.json"));

        QDir temporaryDirectoryPath(temporaryDirectory.path());
        QVERIFY(temporaryDirectoryPath.mkpath(QStringLiteral("storage")));

        LicenseRecord licenseRecord{
            QStringLiteral("MCLD-DISABLE-0001"),
            QStringLiteral("DEVICE-001"),
            true};

        LicenseRepository repository(filePath);
        const LicenseRepositoryResult insertResult = repository.insert(licenseRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(insertResult.errorMessage.isEmpty());

        LicenseManager licenseManager(filePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initializeResult.errorMessage.isEmpty());

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        const LicenseManagerResult result = licenseManager.disableLicense(QStringLiteral("MCLD-DISABLE-0001"));

        QCOMPARE(result.status, LicenseManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(licenseManager.isInitialized());

        LicenseRepository reloadedRepository(movedFilePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        const auto reloadedRecord = reloadedRepository.findByProductKey(licenseRecord.productKey);
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, licenseRecord.productKey);
        QCOMPARE(reloadedRecord->deviceId, licenseRecord.deviceId);
        QVERIFY(reloadedRecord->enabled);
    }

    void enableLicense_existingDisabledLicense_persistsEnabledState()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord originalRecord{
            QStringLiteral("MCLD-ENABLE-0001"),
            QStringLiteral("DEVICE-001"),
            false};

        const LicenseRepositoryResult insertResult = repository.insert(originalRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));

        LicenseManager licenseManager(filePath);

        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(initializeResult.errorMessage, QString());

        const LicenseManagerResult result = licenseManager.enableLicense(QStringLiteral("MCLD-ENABLE-0001"));
        QCOMPARE(result.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(result.errorMessage, QString());
        QVERIFY(licenseManager.isInitialized());

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());
        QVERIFY(reloadedRepository.count() == 1);

        const auto currentRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-ENABLE-0001"));
        QVERIFY(currentRecord.has_value());
        QCOMPARE(currentRecord->productKey, QStringLiteral("MCLD-ENABLE-0001"));
        QCOMPARE(currentRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(currentRecord->enabled, true);
    }

    void enableLicense_alreadyEnabledLicense_succeedsWithoutRewrite()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storagePath = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString movedStoragePath = temporaryDirectory.filePath(QStringLiteral("storage-moved"));
        const QString filePath = QDir(storagePath).filePath(QStringLiteral("licenses.json"));
        const QString movedFilePath = QDir(movedStoragePath).filePath(QStringLiteral("licenses.json"));

        QDir temporaryDirectoryPath(temporaryDirectory.path());
        QVERIFY(temporaryDirectoryPath.mkpath(QStringLiteral("storage")));

        LicenseRecord enabledRecord{
            QStringLiteral("MCLD-ENABLE-0001"),
            QStringLiteral("DEVICE-001"),
            true};

        LicenseRepository repository(filePath);
        const LicenseRepositoryResult insertResult = repository.insert(enabledRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(filePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        const LicenseManagerResult result = licenseManager.enableLicense(QStringLiteral("MCLD-ENABLE-0001"));
        QCOMPARE(result.status, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(licenseManager.isInitialized());

        LicenseRepository reloadedRepository(movedFilePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-ENABLE-0001"));
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-ENABLE-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(reloadedRecord->enabled, true);
    }

    void enableLicense_persistenceFails_returnsFailureWithoutChangingState()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString storagePath = temporaryDirectory.filePath(QStringLiteral("storage"));
        const QString movedStoragePath = temporaryDirectory.filePath(QStringLiteral("storage-moved"));
        const QString filePath = QDir(storagePath).filePath(QStringLiteral("licenses.json"));
        const QString movedFilePath = QDir(movedStoragePath).filePath(QStringLiteral("licenses.json"));

        QDir temporaryDirectoryPath(temporaryDirectory.path());
        QVERIFY(temporaryDirectoryPath.mkpath(QStringLiteral("storage")));

        LicenseRecord disabledRecord{
            QStringLiteral("MCLD-DISABLE-0001"),
            QStringLiteral("DEVICE-001"),
            false};

        LicenseRepository repository(filePath);
        const LicenseRepositoryResult insertResult = repository.insert(disabledRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);

        LicenseManager licenseManager(filePath);
        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        const LicenseManagerResult result = licenseManager.enableLicense(disabledRecord.productKey);
        QCOMPARE(result.status, LicenseManagerOperationStatus::Failed);
        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(licenseManager.isInitialized());

        const AuthenticationResult authResult = licenseManager.authenticate(disabledRecord.productKey, disabledRecord.deviceId);
        QCOMPARE(authResult.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(authResult.authenticationStatus, AuthenticationStatus::Disabled);
        QVERIFY(authResult.errorMessage.isEmpty());

        LicenseRepository reloadedRepository(movedFilePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());

        const auto reloadedRecord = reloadedRepository.findByProductKey(disabledRecord.productKey);
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, disabledRecord.productKey);
        QCOMPARE(reloadedRecord->deviceId, disabledRecord.deviceId);
        QVERIFY(!reloadedRecord->enabled);
    }

    void authenticate_afterDisableAndReEnable_reflectsCurrentLicenseState()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord enabledRecord{
            QStringLiteral("MCLD-ENABLE-0001"),
            QStringLiteral("DEVICE-001"),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(enabledRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));

        LicenseManager licenseManager(filePath);

        const LicenseManagerResult initializeResult = licenseManager.initialize();
        QCOMPARE(initializeResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(initializeResult.errorMessage, QString());

        const AuthenticationResult authResult1 = licenseManager.authenticate(enabledRecord.productKey, enabledRecord.deviceId);
        QCOMPARE(authResult1.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(authResult1.authenticationStatus, AuthenticationStatus::Valid);

        const LicenseManagerResult disableResult = licenseManager.disableLicense(enabledRecord.productKey);
        QCOMPARE(disableResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(disableResult.errorMessage, QString());

        const AuthenticationResult authResult2 = licenseManager.authenticate(enabledRecord.productKey, enabledRecord.deviceId);
        QCOMPARE(authResult2.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(authResult2.authenticationStatus, AuthenticationStatus::Disabled);
        QCOMPARE(authResult2.errorMessage, QString());

        const LicenseManagerResult reenableResult = licenseManager.enableLicense(enabledRecord.productKey);
        QCOMPARE(reenableResult.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(reenableResult.errorMessage, QString());

        const AuthenticationResult authResult3 = licenseManager.authenticate(enabledRecord.productKey, enabledRecord.deviceId);
        QCOMPARE(authResult3.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(authResult3.authenticationStatus, AuthenticationStatus::Valid);

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);

        const auto reloadedRecord = reloadedRepository.findByProductKey(enabledRecord.productKey);
        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, enabledRecord.productKey);
        QCOMPARE(reloadedRecord->deviceId, enabledRecord.deviceId);
        QCOMPARE(reloadedRecord->enabled, true);
    }

    void createLicense_afterInitialization_generatesCanonicalProductKeyFormat()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        LicenseManager manager(filePath);

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        const auto result = manager.createLicense();

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(!result.productKey.isEmpty());

        const QRegularExpression productKeyPattern(QStringLiteral(
            R"(^MCLD-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}$)"));

        QVERIFY(productKeyPattern.isValid());

        const QRegularExpressionMatch match = productKeyPattern.match(result.productKey);

        QVERIFY2(match.hasMatch(), qPrintable(QStringLiteral("Unexpected Product Key format: %1").arg(result.productKey)));
    }

    void createLicense_generatedKeyCollision_retriesAndPreservesLeadingZeroes()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        const LicenseRecord existingRecord{
            QStringLiteral("MCLD-ABCD-1234-DEAD-BEEF"),
            QStringLiteral("DEVICE-ORIGINAL"),
            false};

        LicenseRepository repository(filePath);
        const LicenseRepositoryResult insertResult = repository.insert(existingRecord);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(insertResult.errorMessage.isEmpty());

        const std::array<quint64, 2> entropyValues{
            0xABCD1234DEADBEEFULL,
            0x00010ABCDEF01234ULL};
        int entropyIndex = 0;

        LicenseManager manager(filePath,
                               [&entropyValues, &entropyIndex]() -> quint64
                               {
                                   if (entropyIndex >= static_cast<int>(entropyValues.size()))
                                   {
                                       return 0;
                                   }

                                   return entropyValues[static_cast<std::size_t>(entropyIndex++)];
                               });

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());

        const auto result = manager.createLicense();

        QCOMPARE(result.operationStatus, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.productKey, QStringLiteral("MCLD-0001-0ABC-DEF0-1234"));
        QCOMPARE(entropyIndex, 2);

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();
        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QVERIFY(reloadResult.errorMessage.isEmpty());
        QCOMPARE(reloadedRepository.count(), qsizetype(2));

        const auto reloadedExistingRecord = reloadedRepository.findByProductKey(existingRecord.productKey);
        QVERIFY(reloadedExistingRecord.has_value());
        QCOMPARE(reloadedExistingRecord->productKey, existingRecord.productKey);
        QCOMPARE(reloadedExistingRecord->deviceId, existingRecord.deviceId);
        QCOMPARE(reloadedExistingRecord->enabled, existingRecord.enabled);

        const auto reloadedNewRecord = reloadedRepository.findByProductKey(result.productKey);
        QVERIFY(reloadedNewRecord.has_value());
        QCOMPARE(reloadedNewRecord->productKey, result.productKey);
        QCOMPARE(reloadedNewRecord->deviceId, QString());
        QVERIFY(reloadedNewRecord->enabled);
    }

    void listLicenses_beforeInitialization_fails()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);
        QVERIFY(!manager.isInitialized());

        LicenseListResult result = manager.listLicenses();

        QCOMPARE(result.status, LicenseManagerOperationStatus::Failed);

        QVERIFY(!result.errorMessage.isEmpty());
        QVERIFY(result.licenses.isEmpty());
        QVERIFY(!manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void listLicenses_emptyRepository_returnsEmptyListWithoutMutation()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseManager manager(filePath);

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        LicenseListResult result = manager.listLicenses();

        QCOMPARE(result.status, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QVERIFY(result.licenses.isEmpty());
        QVERIFY(manager.isInitialized());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void listLicenses_multipleRecords_returnsAllFieldsInDeterministicOrderWithoutRewrite()
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

        const QList<LicenseRecord> records{
            {QStringLiteral("MCLD-RECORD-0001"), QStringLiteral("DEVICE-001"), true},
            {QStringLiteral("MCLD-RECORD-0002"), QStringLiteral("DEVICE-002"), false},
            {QStringLiteral("MCLD-RECORD-0003"), QStringLiteral("DEVICE-003"), true}};

        for (const auto &record : records)
        {
            const LicenseRepositoryResult insertResult = repository.insert(record);
            QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
            QVERIFY(insertResult.errorMessage.isEmpty());
        }

        LicenseManager manager(filePath);

        const LicenseManagerResult initResult = manager.initialize();
        QCOMPARE(initResult.status, LicenseManagerOperationStatus::Success);
        QVERIFY(initResult.errorMessage.isEmpty());
        QVERIFY(manager.isInitialized());

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));
        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        LicenseListResult result = manager.listLicenses();

        QCOMPARE(result.status, LicenseManagerOperationStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(result.licenses.size(), records.size());

        for (int i = 0; i < records.size(); ++i)
        {
            const auto &expectedRecord = records[i];
            const auto &actualView = result.licenses[i];

            QCOMPARE(actualView.productKey, expectedRecord.productKey);
            QCOMPARE(actualView.deviceId, expectedRecord.deviceId);
            QCOMPARE(actualView.enabled, expectedRecord.enabled);
        }

        QVERIFY(manager.isInitialized());
    }
};

QTEST_MAIN(LicenseManagerTest)
#include "licensemanagertest.moc"
