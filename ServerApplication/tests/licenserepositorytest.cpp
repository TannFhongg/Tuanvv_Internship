#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "licenserepository.h"
#include "licenserecord.h"
#include <QFileInfo>
#include <QObject>

using MiniCloud::Server::LicenseRecord;
using MiniCloud::Server::LicenseRepository;
using MiniCloud::Server::LicenseRepositoryResult;
using MiniCloud::Server::LicenseRepositoryStatus;

class LicenseRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void load_missingFile_succeedsWithEmptyRepository()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);

        const auto result = repository.load();

        QCOMPARE(result.status, LicenseRepositoryStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{0});

        const auto record = repository.findByProductKey(QStringLiteral("MCLD-NOT-EXIST"));

        QVERIFY(!record.has_value());
        QVERIFY(!QFileInfo::exists(filePath));
    }

    void load_validSingleRecord_populatesRepository()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        const QByteArray jsonData = R"(
{
    "schemaVersion": 1,
    "licenses": [
        {
            "productKey": "MCLD-1234-5678",
            "deviceId": "DEVICE-001",
            "enabled": false
        }
    ]
}
)";

        QCOMPARE(file.write(jsonData), jsonData.size());
        file.close();

        QVERIFY(QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);

        const auto result = repository.load();

        QCOMPARE(result.status, LicenseRepositoryStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{1});

        const auto record = repository.findByProductKey(QStringLiteral("MCLD-1234-5678"));
        QVERIFY(record.has_value());
        QCOMPARE(record->productKey, QStringLiteral("MCLD-1234-5678"));
        QCOMPARE(record->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(record->enabled, false);
    }

    void load_malformedJson_failsWithoutChangingExistingRecords()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        const QByteArray license_A = R"(
{
    "schemaVersion": 1,
    "licenses": [
        {
            "productKey": "MCLD-1234-5678",
            "deviceId": "DEVICE-001",
            "enabled": false
        }
    ]
}
)";

        QCOMPARE(file.write(license_A), license_A.size());
        file.close();

        QVERIFY(QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);
        const auto result1 = repository.load();
        QCOMPARE(result1.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype{1});
        QCOMPARE(result1.errorMessage.isEmpty(), true);

        const QByteArray malformedJson = QByteArrayLiteral(R"({"schemaVersion":1,"licenses":[)");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        QCOMPARE(file.write(malformedJson), malformedJson.size());
        file.close();

        const auto result2 = repository.load();
        QCOMPARE(result2.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!result2.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{1});

        QCOMPARE(result2.status, LicenseRepositoryStatus::Failed);
        const auto record = repository.findByProductKey(QStringLiteral("MCLD-1234-5678"));
        QVERIFY(record.has_value());
        QCOMPARE(record->productKey, QStringLiteral("MCLD-1234-5678"));
        QCOMPARE(record->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(record->enabled, false);
    }

    void load_invalidRootSchema_failsWithoutChangingExistingRecords_data()
    {
        QTest::addColumn<QByteArray>("invalidPayload");
        QTest::addRow("empty-file") << QByteArray();
        QTest::addRow("root-array") << QByteArrayLiteral("[]");
        QTest::addRow("missing-schema-version") << QByteArrayLiteral(R"({"licenses":[]})");
        QTest::addRow("schema-version-wrong-type") << QByteArrayLiteral(R"({"schemaVersion":"1","licenses":[]})");
        QTest::addRow("schema-version-fractional") << QByteArrayLiteral(R"({"schemaVersion":1.5,"licenses":[]})");
        QTest::addRow("unsupported-schema-version") << QByteArrayLiteral(R"({"schemaVersion":2,"licenses":[]})");
        QTest::addRow("missing-licenses") << QByteArrayLiteral(R"({"schemaVersion":1})");
        QTest::addRow("licenses-wrong-type") << QByteArrayLiteral(R"({"schemaVersion":1,"licenses":{}})");
    }

    void load_invalidRootSchema_failsWithoutChangingExistingRecords()
    {
        QFETCH(QByteArray, invalidPayload);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        const QByteArray validPayload = QByteArrayLiteral(R"(
{
    "schemaVersion": 1,
    "licenses": [
        {
            "productKey": "MCLD-LICENSE-A",
            "deviceId": "DEVICE-A",
            "enabled": true
        }
    ]
}
)");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        QCOMPARE(file.write(validPayload), validPayload.size());
        file.close();

        QVERIFY(QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);

        const auto result = repository.load();

        QCOMPARE(result.status, LicenseRepositoryStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{1});

        const auto record = repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A"));
        QVERIFY(record.has_value());

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        QCOMPARE(file.write(invalidPayload), invalidPayload.size());
        file.close();

        const auto result2 = repository.load();
        QCOMPARE(result2.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!result2.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{1});

        const auto record2 = repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A"));
        QVERIFY(record2.has_value());
        QCOMPARE(record2->productKey, QStringLiteral("MCLD-LICENSE-A"));
        QCOMPARE(record2->deviceId, QStringLiteral("DEVICE-A"));
        QCOMPARE(record2->enabled, record->enabled);
    }

    void load_invalidLicenseRecord_failsWithoutChangingExistingRecords_data()
    {
        QTest::addColumn<QByteArray>("invalidRecordJson");

        QTest::newRow("record-not-object") << QByteArrayLiteral("42");

        QTest::newRow("missing-product-key") << QByteArrayLiteral(R"({"deviceId":"DEVICE-X","enabled":true})");

        QTest::newRow("product-key-wrong-type") << QByteArrayLiteral(R"({"productKey":123,"deviceId":"DEVICE-X","enabled":true})");

        QTest::newRow("empty-product-key") << QByteArrayLiteral(R"({"productKey":"","deviceId":"DEVICE-X","enabled":true})");

        QTest::newRow("missing-device-id") << QByteArrayLiteral(R"({"productKey":"MCLD-INVALID","enabled":true})");

        QTest::newRow("device-id-wrong-type") << QByteArrayLiteral(R"({"productKey":"MCLD-INVALID","deviceId":{},"enabled":true})");

        QTest::newRow("missing-enabled") << QByteArrayLiteral(R"({"productKey":"MCLD-INVALID","deviceId":"DEVICE-X"})");

        QTest::newRow("enabled-wrong-type") << QByteArrayLiteral(R"({"productKey":"MCLD-INVALID","deviceId":"DEVICE-X","enabled":"true"})");
    }

    void load_invalidLicenseRecord_failsWithoutChangingExistingRecords()
    {
        QFETCH(QByteArray, invalidRecordJson);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath =
            temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        const QByteArray licenseAPayload = QByteArrayLiteral(
            R"({"schemaVersion":1,"licenses":[{"productKey":"MCLD-LICENSE-A","deviceId":"DEVICE-A","enabled":true}]})");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QCOMPARE(file.write(licenseAPayload), static_cast<qint64>(licenseAPayload.size()));
        file.close();

        LicenseRepository repository(filePath);

        const LicenseRepositoryResult initialResult = repository.load();
        QCOMPARE(initialResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(1));

        const auto licenseABefore = repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A"));
        QVERIFY(licenseABefore.has_value());

        const QByteArray invalidPayload = QByteArrayLiteral(R"({"schemaVersion":1,"licenses":[{"productKey":"MCLD-LICENSE-B","deviceId":"DEVICE-B","enabled":true},)") +
                                          invalidRecordJson + QByteArrayLiteral("]}");

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        QCOMPARE(file.write(invalidPayload), static_cast<qint64>(invalidPayload.size()));
        file.close();

        const LicenseRepositoryResult failedResult = repository.load();

        QCOMPARE(failedResult.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!failedResult.errorMessage.isEmpty());

        QCOMPARE(repository.count(), qsizetype(1));

        const auto licenseAAfter = repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A"));

        QVERIFY(licenseAAfter.has_value());
        QCOMPARE(licenseAAfter->productKey, licenseABefore->productKey);
        QCOMPARE(licenseAAfter->deviceId, licenseABefore->deviceId);
        QCOMPARE(licenseAAfter->enabled, licenseABefore->enabled);

        QVERIFY(!repository.findByProductKey(QStringLiteral("MCLD-LICENSE-B")).has_value());

        QVERIFY(!repository.findByProductKey(QStringLiteral("MCLD-INVALID")).has_value());
    }

    void load_duplicateProductKey_failsWithoutChangingExistingRecords()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        const QByteArray License_A = R"(
{
    "schemaVersion": 1,
    "licenses": [
        {
            "productKey": "MCLD-1234-5678",
            "deviceId": "DEVICE-001",
            "enabled": true
        }
    ]
}
)";

        QCOMPARE(file.write(License_A), License_A.size());
        file.close();

        QVERIFY(QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);

        const auto result = repository.load();

        QCOMPARE(result.status, LicenseRepositoryStatus::Success);
        QVERIFY(result.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{1});

        const QByteArray duplicatePayload = R"(
{
    "schemaVersion": 1,
    "licenses": [
        {
            "productKey": "MCLD-LICENSE-B",
            "deviceId": "DEVICE-001",
            "enabled": true
        },
        {
            "productKey": "MCLD-LICENSE-B",
            "deviceId": "DEVICE-002",
            "enabled": true
        }
    ]
}
)";

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        QCOMPARE(file.write(duplicatePayload), duplicatePayload.size());
        file.close();

        const auto result_B = repository.load();
        QCOMPARE(result_B.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!result_B.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype{1});

        const auto record = repository.findByProductKey(QStringLiteral("MCLD-1234-5678"));
        QVERIFY(record.has_value());
        QCOMPARE(record->productKey, QStringLiteral("MCLD-1234-5678"));
        QCOMPARE(record->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(record->enabled, true);

        const auto duplicateRecord = repository.findByProductKey(QStringLiteral("MCLD-LICENSE-B"));
        QVERIFY(!duplicateRecord.has_value());
    }

    void load_emptyLicenseArray_replacesExistingRepositoryWithEmpty()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        const QByteArray licenseAPayload = QByteArrayLiteral(
            R"({"schemaVersion":1,"licenses":[{"productKey":"MCLD-LICENSE-A","deviceId":"DEVICE-A","enabled":true}]})");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QCOMPARE(file.write(licenseAPayload), static_cast<qint64>(licenseAPayload.size()));
        file.close();

        LicenseRepository repository(filePath);

        const LicenseRepositoryResult firstResult = repository.load();

        QCOMPARE(firstResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(firstResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));
        QVERIFY(repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A")).has_value());

        const QByteArray emptyLicenseArrayPayload =
            QByteArrayLiteral(R"({"schemaVersion":1,"licenses":[]})");

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));

        QCOMPARE(file.write(emptyLicenseArrayPayload), static_cast<qint64>(emptyLicenseArrayPayload.size()));
        file.close();

        const LicenseRepositoryResult secondResult = repository.load();

        QCOMPARE(secondResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(secondResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(0));

        QVERIFY(!repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A")).has_value());
    }

    void load_extraFieldsAndUnboundLicense_succeeds()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        const QByteArray licenseAPayload = QByteArrayLiteral(
            R"({
  "schemaVersion": 1,
  "futureRootField": "ignored",
  "licenses": [
    {
      "productKey": "MCLD-UNBOUND-0001",
      "deviceId": "",
      "enabled": true,
      "futureRecordField": 123
    }
  ]
})");

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QCOMPARE(file.write(licenseAPayload), static_cast<qint64>(licenseAPayload.size()));
        file.close();

        LicenseRepository repository(filePath);

        const LicenseRepositoryResult result = repository.load();

        QCOMPARE(result.status, LicenseRepositoryStatus::Success);
        QCOMPARE(result.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));
        const auto record = repository.findByProductKey(QStringLiteral("MCLD-UNBOUND-0001"));
        QVERIFY(record.has_value());
        QCOMPARE(record->productKey, QStringLiteral("MCLD-UNBOUND-0001"));
        QCOMPARE(record->deviceId, QString());
        QCOMPARE(record->enabled, true);
    }

    void insert_validRecord_persistsAndCanBeReloaded()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const auto loadResult = repository.load();
        QCOMPARE(loadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(0));

        const LicenseRecord record = LicenseRecord{
            QStringLiteral("MCLD-1234-5678"),
            QString(),
            false};

        const auto insertResult = repository.insert(record);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(1));
        QCOMPARE(QFileInfo::exists(filePath), true);

        LicenseRepository reloadedRepository(filePath);
        const auto reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadedRepository.count(), qsizetype(1));

        const auto savedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-1234-5678"));
        QVERIFY(savedRecord.has_value());
        QCOMPARE(savedRecord->productKey, QStringLiteral("MCLD-1234-5678"));
        QCOMPARE(savedRecord->deviceId, QString());
        QCOMPARE(savedRecord->enabled, false);
    }

    void insert_duplicateProductKey_failsWithoutChangingRepositoryOrFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const auto loadResult = repository.load();
        QCOMPARE(loadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(0));

        const LicenseRecord originalRecord = LicenseRecord{
            QStringLiteral("MCLD-1234-5678"),
            QString(),
            false};

        const auto insertResult1 = repository.insert(originalRecord);
        QCOMPARE(insertResult1.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(1));
        QCOMPARE(QFileInfo::exists(filePath), true);

        const LicenseRecord duplicateRecord = LicenseRecord{
            QStringLiteral("MCLD-1234-5678"),
            QStringLiteral("DEVICE-OTHER"),
            true};

        const auto insertResult2 = repository.insert(duplicateRecord);
        QCOMPARE(insertResult2.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!insertResult2.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype(1));

        const auto currentRecord = repository.findByProductKey(QStringLiteral("MCLD-1234-5678"));
        QVERIFY(currentRecord.has_value());
        QCOMPARE(currentRecord->deviceId, QString());
        QCOMPARE(currentRecord->enabled, false);

        LicenseRepository reloadedRepository(filePath);
        const auto reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadedRepository.count(), qsizetype(1));

        const auto savedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-1234-5678"));
        QVERIFY(savedRecord.has_value());
        QCOMPARE(savedRecord->deviceId, QString());
        QCOMPARE(savedRecord->enabled, false);
    }

    void insert_emptyProductKey_failsWithoutCreatingFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const auto loadResult = repository.load();
        QCOMPARE(loadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(0));

        const LicenseRecord record = LicenseRecord{
            QString(),
            QString(),
            false};

        const auto insertResult = repository.insert(record);
        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!insertResult.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype(0));
        QCOMPARE(QFileInfo::exists(filePath), false);
    }

    void insert_fileOpenFails_doesNotChangeMemory()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString missingParentPath = temporaryDirectory.filePath(QStringLiteral("missing-parent"));

        const QString filePath = QDir(missingParentPath).filePath(QStringLiteral("licenses.json"));

        QVERIFY(!QDir(missingParentPath).exists());
        QVERIFY(!QFileInfo::exists(filePath));

        LicenseRepository repository(filePath);

        const LicenseRepositoryResult loadResult = repository.load();

        QCOMPARE(loadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(loadResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(0));

        const LicenseRecord record{
            QStringLiteral("MCLD-1234-5678"),
            QString(),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(record);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!insertResult.errorMessage.isEmpty());

        QCOMPARE(repository.count(), qsizetype(0));

        QVERIFY(!repository.findByProductKey(QStringLiteral("MCLD-1234-5678")).has_value());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(!QDir(missingParentPath).exists());
    }

    void update_existingRecord_persistsAndCanBeReloaded()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord originalRecord{
            QStringLiteral("MCLD-UPDATE-0001"),
            QString(),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(originalRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));

        const LicenseRecord updatedRecord{
            QStringLiteral("MCLD-UPDATE-0001"),
            QStringLiteral("DEVICE-001"),
            false};

        const LicenseRepositoryResult updateResult = repository.update(updatedRecord);

        QCOMPARE(updateResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(updateResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));

        const auto currentRecord = repository.findByProductKey(
            QStringLiteral("MCLD-UPDATE-0001"));

        QVERIFY(currentRecord.has_value());
        QCOMPARE(currentRecord->productKey, QStringLiteral("MCLD-UPDATE-0001"));
        QCOMPARE(currentRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(currentRecord->enabled, false);

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        QCOMPARE(reloadedRepository.count(), qsizetype(1));

        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-UPDATE-0001"));

        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-UPDATE-0001"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-001"));
        QCOMPARE(reloadedRecord->enabled, false);
    }

    void update_invalidTarget_failsWithoutChangingRepositoryOrFile_data()
    {
        QTest::addColumn<QString>("productKey");

        QTest::newRow("empty-product-key") << QString();
        QTest::newRow("unknown-product-key") << QStringLiteral("MCLD-NOT-EXIST");
    }

    void update_invalidTarget_failsWithoutChangingRepositoryOrFile()
    {
        QFETCH(QString, productKey);

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord originalRecord{
            QStringLiteral("MCLD-LICENSE-A"),
            QStringLiteral("DEVICE-ORIGINAL"),
            true};

        const LicenseRepositoryResult insertResult =
            repository.insert(originalRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(1));

        const LicenseRecord updatedRecord{
            productKey,
            QStringLiteral("DEVICE-CHANGED"),
            false};

        const LicenseRepositoryResult updateResult =
            repository.update(updatedRecord);

        QCOMPARE(updateResult.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!updateResult.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype(1));

        const auto currentRecord = repository.findByProductKey(QStringLiteral("MCLD-LICENSE-A"));

        QVERIFY(currentRecord.has_value());
        QCOMPARE(currentRecord->productKey, QStringLiteral("MCLD-LICENSE-A"));
        QCOMPARE(currentRecord->deviceId, QStringLiteral("DEVICE-ORIGINAL"));
        QCOMPARE(currentRecord->enabled, true);

        QVERIFY(!repository.findByProductKey(productKey).has_value());

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        QCOMPARE(reloadedRepository.count(), qsizetype(1));

        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-LICENSE-A"));

        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-LICENSE-A"));
        QCOMPARE(reloadedRecord->deviceId, QStringLiteral("DEVICE-ORIGINAL"));
        QCOMPARE(reloadedRecord->enabled, true);

        QVERIFY(!reloadedRepository.findByProductKey(productKey).has_value());
    }

    void insert_secondRecord_preservesExistingRecordAfterReload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord licenseA{
            QStringLiteral("MCLD-MULTI-0001"),
            QString(),
            true};

        const LicenseRecord licenseB{
            QStringLiteral("MCLD-MULTI-0002"),
            QStringLiteral("DEVICE-002"),
            false};

        const LicenseRepositoryResult insertAResult = repository.insert(licenseA);
        const LicenseRepositoryResult insertBResult = repository.insert(licenseB);

        QCOMPARE(insertAResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertAResult.errorMessage, QString());
        QCOMPARE(insertBResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertBResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(2));

        const auto currentLicenseA = repository.findByProductKey(QStringLiteral("MCLD-MULTI-0001"));
        const auto currentLicenseB = repository.findByProductKey(QStringLiteral("MCLD-MULTI-0002"));

        QVERIFY(currentLicenseA.has_value());
        QCOMPARE(currentLicenseA->productKey, QStringLiteral("MCLD-MULTI-0001"));
        QCOMPARE(currentLicenseA->deviceId, QString());
        QCOMPARE(currentLicenseA->enabled, true);

        QVERIFY(currentLicenseB.has_value());
        QCOMPARE(currentLicenseB->productKey, QStringLiteral("MCLD-MULTI-0002"));
        QCOMPARE(currentLicenseB->deviceId, QStringLiteral("DEVICE-002"));
        QCOMPARE(currentLicenseB->enabled, false);

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        QCOMPARE(reloadedRepository.count(), qsizetype(2));

        const auto reloadedLicenseA = reloadedRepository.findByProductKey(QStringLiteral("MCLD-MULTI-0001"));
        const auto reloadedLicenseB = reloadedRepository.findByProductKey(QStringLiteral("MCLD-MULTI-0002"));

        QVERIFY(reloadedLicenseA.has_value());
        QCOMPARE(reloadedLicenseA->productKey, QStringLiteral("MCLD-MULTI-0001"));
        QCOMPARE(reloadedLicenseA->deviceId, QString());
        QCOMPARE(reloadedLicenseA->enabled, true);

        QVERIFY(reloadedLicenseB.has_value());
        QCOMPARE(reloadedLicenseB->productKey, QStringLiteral("MCLD-MULTI-0002"));
        QCOMPARE(reloadedLicenseB->deviceId, QStringLiteral("DEVICE-002"));
        QCOMPARE(reloadedLicenseB->enabled, false);
    }

    void update_oneRecord_preservesOtherRecordAfterReload()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString filePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));

        LicenseRepository repository(filePath);

        const LicenseRecord licenseA{
            QStringLiteral("MCLD-UPDATE-A"),
            QString(),
            true};

        const LicenseRecord licenseB{
            QStringLiteral("MCLD-UPDATE-B"),
            QStringLiteral("DEVICE-B"),
            false};

        const LicenseRepositoryResult insertAResult = repository.insert(licenseA);
        const LicenseRepositoryResult insertBResult = repository.insert(licenseB);

        QCOMPARE(insertAResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertBResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(repository.count(), qsizetype(2));

        const LicenseRecord updatedLicenseA{
            QStringLiteral("MCLD-UPDATE-A"),
            QStringLiteral("DEVICE-A"),
            false};

        const LicenseRepositoryResult updateResult = repository.update(updatedLicenseA);

        QCOMPARE(updateResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(updateResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(2));

        const auto currentLicenseA = repository.findByProductKey(QStringLiteral("MCLD-UPDATE-A"));
        const auto currentLicenseB = repository.findByProductKey(QStringLiteral("MCLD-UPDATE-B"));

        QVERIFY(currentLicenseA.has_value());
        QCOMPARE(currentLicenseA->productKey, QStringLiteral("MCLD-UPDATE-A"));
        QCOMPARE(currentLicenseA->deviceId, QStringLiteral("DEVICE-A"));
        QCOMPARE(currentLicenseA->enabled, false);

        QVERIFY(currentLicenseB.has_value());
        QCOMPARE(currentLicenseB->productKey, QStringLiteral("MCLD-UPDATE-B"));
        QCOMPARE(currentLicenseB->deviceId, QStringLiteral("DEVICE-B"));
        QCOMPARE(currentLicenseB->enabled, false);

        LicenseRepository reloadedRepository(filePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        QCOMPARE(reloadedRepository.count(), qsizetype(2));

        const auto reloadedLicenseA = reloadedRepository.findByProductKey(QStringLiteral("MCLD-UPDATE-A"));
        const auto reloadedLicenseB = reloadedRepository.findByProductKey(QStringLiteral("MCLD-UPDATE-B"));

        QVERIFY(reloadedLicenseA.has_value());
        QCOMPARE(reloadedLicenseA->productKey, QStringLiteral("MCLD-UPDATE-A"));
        QCOMPARE(reloadedLicenseA->deviceId, QStringLiteral("DEVICE-A"));
        QCOMPARE(reloadedLicenseA->enabled, false);

        QVERIFY(reloadedLicenseB.has_value());
        QCOMPARE(reloadedLicenseB->productKey, QStringLiteral("MCLD-UPDATE-B"));
        QCOMPARE(reloadedLicenseB->deviceId, QStringLiteral("DEVICE-B"));
        QCOMPARE(reloadedLicenseB->enabled, false);
    }

    void update_fileOpenFails_doesNotChangeMemoryOrPersistedRecord()
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

        const LicenseRecord originalRecord{
            QStringLiteral("MCLD-ROLLBACK-0001"),
            QString(),
            true};

        const LicenseRepositoryResult insertResult = repository.insert(originalRecord);

        QCOMPARE(insertResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(insertResult.errorMessage, QString());
        QCOMPARE(repository.count(), qsizetype(1));
        QVERIFY(QFileInfo::exists(filePath));

        QVERIFY(temporaryDirectoryPath.rename(QStringLiteral("storage"), QStringLiteral("storage-moved")));

        QVERIFY(!QDir(storagePath).exists());
        QVERIFY(!QFileInfo::exists(filePath));
        QVERIFY(QFileInfo::exists(movedFilePath));

        const LicenseRecord updatedRecord{
            QStringLiteral("MCLD-ROLLBACK-0001"),
            QStringLiteral("DEVICE-CHANGED"),
            false};

        const LicenseRepositoryResult updateResult = repository.update(updatedRecord);

        QCOMPARE(updateResult.status, LicenseRepositoryStatus::Failed);
        QVERIFY(!updateResult.errorMessage.isEmpty());
        QCOMPARE(repository.count(), qsizetype(1));

        const auto currentRecord = repository.findByProductKey(QStringLiteral("MCLD-ROLLBACK-0001"));

        QVERIFY(currentRecord.has_value());
        QCOMPARE(currentRecord->deviceId, QString());
        QCOMPARE(currentRecord->enabled, true);
        
        LicenseRepository reloadedRepository(movedFilePath);
        const LicenseRepositoryResult reloadResult = reloadedRepository.load();

        QCOMPARE(reloadResult.status, LicenseRepositoryStatus::Success);
        QCOMPARE(reloadResult.errorMessage, QString());
        QCOMPARE(reloadedRepository.count(), qsizetype(1));

        const auto reloadedRecord = reloadedRepository.findByProductKey(QStringLiteral("MCLD-ROLLBACK-0001"));

        QVERIFY(reloadedRecord.has_value());
        QCOMPARE(reloadedRecord->productKey, QStringLiteral("MCLD-ROLLBACK-0001"));
        QCOMPARE(reloadedRecord->deviceId, QString());
        QCOMPARE(reloadedRecord->enabled, true);
    }

};

QTEST_MAIN(LicenseRepositoryTest)
#include "licenserepositorytest.moc"
