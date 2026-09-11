#include <algorithm>

#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

#include "licensemanager.h"
#include "serverwindow.h"

class ServerWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void adminActions_createThenDisable_persistAcrossManagerReload()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString licenseStoragePath = temporaryDirectory.filePath(QStringLiteral("licenses.json"));
        LicenseManager licenseManager(licenseStoragePath, []() { return quint64{1}; });
        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *createLicenseButton = window.findChild<QPushButton *>("createLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(createLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);

        QTest::mouseClick(createLicenseButton, Qt::LeftButton);

        const int createdLicenseRow = licenseTable->currentRow();
        QVERIFY(createdLicenseRow >= 0);
        QVERIFY(licenseTable->item(createdLicenseRow, 0)->isSelected());
        const QString productKey = licenseTable->item(createdLicenseRow, 0)->data(Qt::UserRole).toString();
        QVERIFY(!productKey.isEmpty());
        QVERIFY(disableLicenseButton->isEnabled());

        QTest::mouseClick(disableLicenseButton, Qt::LeftButton);

        QCOMPARE(licenseTable->item(createdLicenseRow, 0)->text(), productKey);
        QCOMPARE(licenseTable->item(createdLicenseRow, 2)->text(), QStringLiteral("Disabled"));

        LicenseManager reloadedLicenseManager(licenseStoragePath);
        QCOMPARE(reloadedLicenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        const auto reloadedLicenses = reloadedLicenseManager.listLicenses();
        QCOMPARE(reloadedLicenses.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(reloadedLicenses.licenses.size(), qsizetype{1});

        const auto persistedLicense = std::find_if(
            reloadedLicenses.licenses.cbegin(),
            reloadedLicenses.licenses.cend(),
            [&productKey](const MiniCloud::Server::LicenseView &license)
            {
                return license.productKey == productKey;
            });
        QVERIFY(persistedLicense != reloadedLicenses.licenses.cend());
        QVERIFY(!persistedLicense->enabled);
        QCOMPARE(persistedLicense->deviceId, QString());
    }

    void createLicenseButton_uninitializedManager_showsErrorWithoutChangingTable()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        LicenseManager licenseManager(temporaryDirectory.filePath(QStringLiteral("licenses.json")));
        const auto createResult = licenseManager.createLicense();
        QCOMPARE(createResult.operationStatus, LicenseManagerOperationStatus::Failed);
        QCOMPARE(createResult.errorMessage, QStringLiteral("LicenseManager is not initialized."));

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *errorLabel = window.findChild<QLabel *>("errorLabel");
        auto *createLicenseButton = window.findChild<QPushButton *>("createLicenseButton");
        auto *enableLicenseButton = window.findChild<QPushButton *>("enableLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(errorLabel != nullptr);
        QVERIFY(createLicenseButton != nullptr);
        QVERIFY(enableLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);

        const int rowCountBeforeClick = licenseTable->rowCount();
        QCOMPARE(rowCountBeforeClick, 0);
        QCOMPARE(errorLabel->text(), createResult.errorMessage);
        QVERIFY(!createLicenseButton->isEnabled());
        QVERIFY(!enableLicenseButton->isEnabled());
        QVERIFY(!disableLicenseButton->isEnabled());
        QVERIFY(licenseTable->selectedItems().isEmpty());

        QTest::mouseClick(createLicenseButton, Qt::LeftButton);

        QCOMPARE(licenseTable->rowCount(), rowCountBeforeClick);
        QCOMPARE(errorLabel->text(), createResult.errorMessage);
        QVERIFY(licenseTable->selectedItems().isEmpty());
        QVERIFY(!enableLicenseButton->isEnabled());
        QVERIFY(!disableLicenseButton->isEnabled());
    }

    void refreshLicensesButton_reloadsManagerListAndPreservesSelectedKey()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        QList<quint64> entropyValues{quint64{1}, quint64{2}, quint64{3}};
        LicenseManager licenseManager(
            temporaryDirectory.filePath(QStringLiteral("licenses.json")),
            [&entropyValues]()
            {
                return entropyValues.takeFirst();
            });

        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        const auto licenseA = licenseManager.createLicense();
        const auto licenseB = licenseManager.createLicense();
        QCOMPARE(licenseA.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(licenseB.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.disableLicense(licenseB.productKey).status,
            LicenseManagerOperationStatus::Success);

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *refreshLicensesButton = window.findChild<QPushButton *>("refreshLicensesButton");
        auto *enableLicenseButton = window.findChild<QPushButton *>("enableLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(refreshLicensesButton != nullptr);
        QVERIFY(enableLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);

        int licenseBRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == licenseB.productKey)
            {
                licenseBRow = row;
                break;
            }
        }

        QVERIFY(licenseBRow >= 0);
        licenseTable->setCurrentCell(licenseBRow, 0);
        licenseTable->selectRow(licenseBRow);
        QVERIFY(enableLicenseButton->isEnabled());
        QVERIFY(!disableLicenseButton->isEnabled());

        const auto licenseC = licenseManager.createLicense();
        QCOMPARE(licenseC.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.disableLicense(licenseA.productKey).status,
            LicenseManagerOperationStatus::Success);
        const auto licensesBeforeRefresh = licenseManager.listLicenses();
        QCOMPARE(licensesBeforeRefresh.status, LicenseManagerOperationStatus::Success);

        QTest::mouseClick(refreshLicensesButton, Qt::LeftButton);

        const auto licensesAfterRefresh = licenseManager.listLicenses();
        QCOMPARE(licensesAfterRefresh.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(licensesAfterRefresh.licenses.size(), licensesBeforeRefresh.licenses.size());
        QCOMPARE(licenseTable->rowCount(), static_cast<int>(licensesAfterRefresh.licenses.size()));

        for (const MiniCloud::Server::LicenseView &license : licensesAfterRefresh.licenses)
        {
            const auto licenseBeforeRefresh = std::find_if(
                licensesBeforeRefresh.licenses.cbegin(),
                licensesBeforeRefresh.licenses.cend(),
                [&license](const MiniCloud::Server::LicenseView &beforeRefresh)
                {
                    return beforeRefresh.productKey == license.productKey;
                });
            QVERIFY(licenseBeforeRefresh != licensesBeforeRefresh.licenses.cend());
            QCOMPARE(license.deviceId, licenseBeforeRefresh->deviceId);
            QCOMPARE(license.enabled, licenseBeforeRefresh->enabled);

            int tableRow = -1;
            for (int row = 0; row < licenseTable->rowCount(); ++row)
            {
                if (licenseTable->item(row, 0)->text() == license.productKey)
                {
                    tableRow = row;
                    break;
                }
            }

            QVERIFY(tableRow >= 0);
            QCOMPARE(licenseTable->item(tableRow, 1)->text(), license.deviceId);
            QCOMPARE(
                licenseTable->item(tableRow, 2)->text(),
                license.enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled"));
        }

        int refreshedLicenseBRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == licenseB.productKey)
            {
                refreshedLicenseBRow = row;
                break;
            }
        }

        QVERIFY(refreshedLicenseBRow >= 0);
        QCOMPARE(licenseTable->currentRow(), refreshedLicenseBRow);
        QVERIFY(licenseTable->item(refreshedLicenseBRow, 0)->isSelected());
        QVERIFY(enableLicenseButton->isEnabled());
        QVERIFY(!disableLicenseButton->isEnabled());
    }

    void disableLicenseButton_selectedEnabledLicense_disablesAndRefreshesSelection()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        QList<quint64> entropyValues{quint64{1}, quint64{2}, quint64{3}};
        LicenseManager licenseManager(
            temporaryDirectory.filePath(QStringLiteral("licenses.json")),
            [&entropyValues]()
            {
                return entropyValues.takeFirst();
            });

        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        const auto firstLicense = licenseManager.createLicense();
        const auto enabledLicense = licenseManager.createLicense();
        const auto thirdLicense = licenseManager.createLicense();
        QCOMPARE(firstLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(enabledLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(thirdLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.authenticate(enabledLicense.productKey, QStringLiteral("DEVICE-DISABLE")).operationStatus,
            LicenseManagerOperationStatus::Success);

        const auto licensesBeforeDisabling = licenseManager.listLicenses();
        QCOMPARE(licensesBeforeDisabling.status, LicenseManagerOperationStatus::Success);

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *enableLicenseButton = window.findChild<QPushButton *>("enableLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(enableLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);

        int enabledLicenseRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == enabledLicense.productKey)
            {
                enabledLicenseRow = row;
                break;
            }
        }

        QVERIFY(enabledLicenseRow >= 0);
        licenseTable->setCurrentCell(enabledLicenseRow, 0);
        licenseTable->selectRow(enabledLicenseRow);
        QVERIFY(!enableLicenseButton->isEnabled());
        QVERIFY(disableLicenseButton->isEnabled());

        QTest::mouseClick(disableLicenseButton, Qt::LeftButton);

        const auto licensesAfterDisabling = licenseManager.listLicenses();
        QCOMPARE(licensesAfterDisabling.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(licensesAfterDisabling.licenses.size(), licensesBeforeDisabling.licenses.size());
        QCOMPARE(licenseTable->rowCount(), static_cast<int>(licensesAfterDisabling.licenses.size()));

        for (const MiniCloud::Server::LicenseView &license : licensesAfterDisabling.licenses)
        {
            const auto licenseBeforeDisabling = std::find_if(
                licensesBeforeDisabling.licenses.cbegin(),
                licensesBeforeDisabling.licenses.cend(),
                [&license](const MiniCloud::Server::LicenseView &beforeDisabling)
                {
                    return beforeDisabling.productKey == license.productKey;
                });
            QVERIFY(licenseBeforeDisabling != licensesBeforeDisabling.licenses.cend());
            QCOMPARE(license.deviceId, licenseBeforeDisabling->deviceId);

            if (license.productKey == enabledLicense.productKey)
            {
                QVERIFY(!license.enabled);
            }
            else
            {
                QCOMPARE(license.enabled, licenseBeforeDisabling->enabled);
            }
        }

        int disabledLicenseRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == enabledLicense.productKey)
            {
                disabledLicenseRow = row;
                break;
            }
        }

        QVERIFY(disabledLicenseRow >= 0);
        QCOMPARE(licenseTable->item(disabledLicenseRow, 2)->text(), QStringLiteral("Disabled"));
        QCOMPARE(licenseTable->currentRow(), disabledLicenseRow);
        QVERIFY(licenseTable->item(disabledLicenseRow, 0)->isSelected());
        QVERIFY(!disableLicenseButton->isEnabled());
        QVERIFY(enableLicenseButton->isEnabled());
    }

    void enableLicenseButton_selectedDisabledLicense_enablesAndRefreshesSelection()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        QList<quint64> entropyValues{quint64{1}, quint64{2}, quint64{3}};
        LicenseManager licenseManager(
            temporaryDirectory.filePath(QStringLiteral("licenses.json")),
            [&entropyValues]()
            {
                return entropyValues.takeFirst();
            });

        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        const auto firstLicense = licenseManager.createLicense();
        const auto disabledLicense = licenseManager.createLicense();
        const auto thirdLicense = licenseManager.createLicense();
        QCOMPARE(firstLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(disabledLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(thirdLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.disableLicense(disabledLicense.productKey).status,
            LicenseManagerOperationStatus::Success);

        const auto licensesBeforeEnabling = licenseManager.listLicenses();
        QCOMPARE(licensesBeforeEnabling.status, LicenseManagerOperationStatus::Success);

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *enableLicenseButton = window.findChild<QPushButton *>("enableLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(enableLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);

        int disabledLicenseRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == disabledLicense.productKey)
            {
                disabledLicenseRow = row;
                break;
            }
        }

        QVERIFY(disabledLicenseRow >= 0);
        licenseTable->setCurrentCell(disabledLicenseRow, 0);
        licenseTable->selectRow(disabledLicenseRow);
        QVERIFY(enableLicenseButton->isEnabled());
        QVERIFY(!disableLicenseButton->isEnabled());

        QTest::mouseClick(enableLicenseButton, Qt::LeftButton);

        const auto licensesAfterEnabling = licenseManager.listLicenses();
        QCOMPARE(licensesAfterEnabling.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(licensesAfterEnabling.licenses.size(), licensesBeforeEnabling.licenses.size());

        for (const MiniCloud::Server::LicenseView &license : licensesAfterEnabling.licenses)
        {
            const auto licenseBeforeEnabling = std::find_if(
                licensesBeforeEnabling.licenses.cbegin(),
                licensesBeforeEnabling.licenses.cend(),
                [&license](const MiniCloud::Server::LicenseView &beforeEnabling)
                {
                    return beforeEnabling.productKey == license.productKey;
                });
            QVERIFY(licenseBeforeEnabling != licensesBeforeEnabling.licenses.cend());
            QCOMPARE(license.deviceId, licenseBeforeEnabling->deviceId);

            if (license.productKey == disabledLicense.productKey)
            {
                QVERIFY(license.enabled);
            }
            else
            {
                QCOMPARE(license.enabled, licenseBeforeEnabling->enabled);
            }
        }

        int enabledLicenseRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == disabledLicense.productKey)
            {
                enabledLicenseRow = row;
                break;
            }
        }

        QCOMPARE(licenseTable->item(enabledLicenseRow, 2)->text(), QStringLiteral("Enabled"));
        QCOMPARE(licenseTable->currentRow(), enabledLicenseRow);
        QVERIFY(licenseTable->item(enabledLicenseRow, 0)->isSelected());
        QVERIFY(!enableLicenseButton->isEnabled());
        QVERIFY(disableLicenseButton->isEnabled());
    }

    void createLicenseButton_addsGeneratedEnabledLicenseToListAndSelectsIt()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        QList<quint64> entropyValues{quint64{1}, quint64{2}};
        LicenseManager licenseManager(
            temporaryDirectory.filePath(QStringLiteral("licenses.json")),
            [&entropyValues]()
            {
                return entropyValues.takeFirst();
            });

        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);
        QCOMPARE(licenseManager.createLicense().operationStatus, LicenseManagerOperationStatus::Success);

        const auto initialLicenses = licenseManager.listLicenses();
        QCOMPARE(initialLicenses.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(initialLicenses.licenses.size(), qsizetype{1});

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *createLicenseButton = window.findChild<QPushButton *>("createLicenseButton");
        auto *enableLicenseButton = window.findChild<QPushButton *>("enableLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(createLicenseButton != nullptr);
        QVERIFY(enableLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);
        QCOMPARE(licenseTable->rowCount(), static_cast<int>(initialLicenses.licenses.size()));

        QTest::mouseClick(createLicenseButton, Qt::LeftButton);

        const auto licensesAfterCreation = licenseManager.listLicenses();
        QCOMPARE(licensesAfterCreation.status, LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licensesAfterCreation.licenses.size(),
            initialLicenses.licenses.size() + qsizetype{1});
        QCOMPARE(licenseTable->rowCount(), static_cast<int>(licensesAfterCreation.licenses.size()));

        const auto createdLicense = std::find_if(
            licensesAfterCreation.licenses.cbegin(),
            licensesAfterCreation.licenses.cend(),
            [&initialLicenses](const MiniCloud::Server::LicenseView &license)
            {
                return std::none_of(
                    initialLicenses.licenses.cbegin(),
                    initialLicenses.licenses.cend(),
                    [&license](const MiniCloud::Server::LicenseView &initialLicense)
                    {
                        return initialLicense.productKey == license.productKey;
                    });
            });
        QVERIFY(createdLicense != licensesAfterCreation.licenses.cend());

        const QString &productKey = createdLicense->productKey;
        QVERIFY(!productKey.isEmpty());
        QVERIFY(
            QRegularExpression(
                QStringLiteral("^MCLD-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}$"))
                .match(productKey)
                .hasMatch());
        QCOMPARE(createdLicense->deviceId, QString());
        QVERIFY(createdLicense->enabled);

        int createdRow = -1;
        for (int row = 0; row < licenseTable->rowCount(); ++row)
        {
            if (licenseTable->item(row, 0)->text() == productKey)
            {
                createdRow = row;
                break;
            }
        }

        QVERIFY(createdRow >= 0);
        QCOMPARE(licenseTable->item(createdRow, 0)->text(), productKey);
        QCOMPARE(licenseTable->item(createdRow, 1)->text(), QString());
        QCOMPARE(licenseTable->item(createdRow, 2)->text(), QStringLiteral("Enabled"));
        QCOMPARE(licenseTable->currentRow(), createdRow);
        QVERIFY(licenseTable->item(createdRow, 0)->isSelected());
        QVERIFY(disableLicenseButton->isEnabled());
        QVERIFY(!enableLicenseButton->isEnabled());
    }

    void initialState_listsLicensesAndDisablesSelectionActions()
    {
        using MiniCloud::Server::LicenseManager;
        using MiniCloud::Server::LicenseManagerOperationStatus;

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        QList<quint64> entropyValues{quint64{1}, quint64{2}, quint64{3}};
        LicenseManager licenseManager(
            temporaryDirectory.filePath(QStringLiteral("licenses.json")),
            [&entropyValues]()
            {
                return entropyValues.takeFirst();
            });

        QCOMPARE(licenseManager.initialize().status, LicenseManagerOperationStatus::Success);

        const auto firstLicense = licenseManager.createLicense();
        const auto secondLicense = licenseManager.createLicense();
        const auto thirdLicense = licenseManager.createLicense();
        QCOMPARE(firstLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(secondLicense.operationStatus, LicenseManagerOperationStatus::Success);
        QCOMPARE(thirdLicense.operationStatus, LicenseManagerOperationStatus::Success);

        QCOMPARE(
            licenseManager.authenticate(firstLicense.productKey, QStringLiteral("DEVICE-ALPHA")).operationStatus,
            LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.authenticate(secondLicense.productKey, QStringLiteral("DEVICE-BRAVO")).operationStatus,
            LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.authenticate(thirdLicense.productKey, QStringLiteral("DEVICE-CHARLIE")).operationStatus,
            LicenseManagerOperationStatus::Success);
        QCOMPARE(
            licenseManager.disableLicense(secondLicense.productKey).status,
            LicenseManagerOperationStatus::Success);

        ServerWindow window(licenseManager);
        window.show();

        auto *licenseTable = window.findChild<QTableWidget *>("licenseTable");
        auto *createLicenseButton = window.findChild<QPushButton *>("createLicenseButton");
        auto *enableLicenseButton = window.findChild<QPushButton *>("enableLicenseButton");
        auto *disableLicenseButton = window.findChild<QPushButton *>("disableLicenseButton");

        QVERIFY(licenseTable != nullptr);
        QVERIFY(createLicenseButton != nullptr);
        QVERIFY(enableLicenseButton != nullptr);
        QVERIFY(disableLicenseButton != nullptr);

        QCOMPARE(licenseTable->columnCount(), 3);
        QCOMPARE(licenseTable->horizontalHeaderItem(0)->text(), QStringLiteral("Product Key"));
        QCOMPARE(licenseTable->horizontalHeaderItem(1)->text(), QStringLiteral("Device ID"));
        QCOMPARE(licenseTable->horizontalHeaderItem(2)->text(), QStringLiteral("Status"));
        QCOMPARE(licenseTable->rowCount(), 3);

        QCOMPARE(licenseTable->item(0, 0)->text(), QStringLiteral("MCLD-0000-0000-0000-0001"));
        QCOMPARE(licenseTable->item(0, 1)->text(), QStringLiteral("DEVICE-ALPHA"));
        QCOMPARE(licenseTable->item(0, 2)->text(), QStringLiteral("Enabled"));
        QCOMPARE(licenseTable->item(1, 0)->text(), QStringLiteral("MCLD-0000-0000-0000-0002"));
        QCOMPARE(licenseTable->item(1, 1)->text(), QStringLiteral("DEVICE-BRAVO"));
        QCOMPARE(licenseTable->item(1, 2)->text(), QStringLiteral("Disabled"));
        QCOMPARE(licenseTable->item(2, 0)->text(), QStringLiteral("MCLD-0000-0000-0000-0003"));
        QCOMPARE(licenseTable->item(2, 1)->text(), QStringLiteral("DEVICE-CHARLIE"));
        QCOMPARE(licenseTable->item(2, 2)->text(), QStringLiteral("Enabled"));

        QVERIFY(createLicenseButton->isEnabled());
        QVERIFY(!enableLicenseButton->isEnabled());
        QVERIFY(!disableLicenseButton->isEnabled());
    }
};

QTEST_MAIN(ServerWindowTest)

#include "serverwindowtest.moc"
