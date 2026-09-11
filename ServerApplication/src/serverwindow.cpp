#include "serverwindow.h"

#include "licensemanager.h"
#include "ui_serverwindow.h"

#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

ServerWindow::ServerWindow(MiniCloud::Server::LicenseManager &licenseManager, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ServerWindow), m_licenseManager(&licenseManager)
{
    ui->setupUi(this);

    connect(
        ui->licenseTable,
        &QTableWidget::itemSelectionChanged,
        this,
        [this]()
        {
            refreshSelectionActions();
        });

    connect(
        ui->refreshLicensesButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            refreshLicenses();
        });

    connect(
        ui->createLicenseButton,
        &QPushButton::clicked,
        this,
        &ServerWindow::createLicense);

    connect(
        ui->enableLicenseButton,
        &QPushButton::clicked,
        this,
        &ServerWindow::enableSelectedLicense);

    connect(
        ui->disableLicenseButton,
        &QPushButton::clicked,
        this,
        &ServerWindow::disableSelectedLicense);

    refreshLicenses();
}

ServerWindow::~ServerWindow()
{
    delete ui;
}

void ServerWindow::createLicense()
{
    const MiniCloud::Server::CreateLicenseResult result = m_licenseManager->createLicense();
    if (result.operationStatus != MiniCloud::Server::LicenseManagerOperationStatus::Success)
    {
        ui->errorLabel->setText(result.errorMessage);
        return;
    }

    refreshLicenses(result.productKey);
}

void ServerWindow::enableSelectedLicense()
{
    const int selectedRow = ui->licenseTable->currentRow();
    const QTableWidgetItem *productKeyItem = selectedRow >= 0
                                                  ? ui->licenseTable->item(selectedRow, 0)
                                                  : nullptr;
    const QString productKey = productKeyItem ? productKeyItem->data(Qt::UserRole).toString() : QString();

    if (productKey.isEmpty())
    {
        ui->errorLabel->setText(tr("Select a license to enable."));
        return;
    }

    const MiniCloud::Server::LicenseManagerResult result = m_licenseManager->enableLicense(productKey);
    if (result.status != MiniCloud::Server::LicenseManagerOperationStatus::Success)
    {
        ui->errorLabel->setText(result.errorMessage);
        return;
    }

    refreshLicenses(productKey);
}

void ServerWindow::disableSelectedLicense()
{
    const int selectedRow = ui->licenseTable->currentRow();
    const QTableWidgetItem *productKeyItem = selectedRow >= 0
                                                  ? ui->licenseTable->item(selectedRow, 0)
                                                  : nullptr;
    const QString productKey = productKeyItem ? productKeyItem->data(Qt::UserRole).toString() : QString();

    if (productKey.isEmpty())
    {
        ui->errorLabel->setText(tr("Select a license to disable."));
        return;
    }

    const MiniCloud::Server::LicenseManagerResult result = m_licenseManager->disableLicense(productKey);
    if (result.status != MiniCloud::Server::LicenseManagerOperationStatus::Success)
    {
        ui->errorLabel->setText(result.errorMessage);
        return;
    }

    refreshLicenses(productKey);
}

void ServerWindow::refreshLicenses(const QString &preferredProductKey)
{
    QString productKeyToSelect = preferredProductKey;
    if (productKeyToSelect.isEmpty() && !ui->licenseTable->selectedItems().isEmpty())
    {
        const int selectedRow = ui->licenseTable->currentRow();
        const QTableWidgetItem *productKeyItem = selectedRow >= 0
                                                      ? ui->licenseTable->item(selectedRow, 0)
                                                      : nullptr;
        if (productKeyItem)
        {
            productKeyToSelect = productKeyItem->data(Qt::UserRole).toString();
        }
    }

    const MiniCloud::Server::LicenseListResult result = m_licenseManager->listLicenses();

    ui->licenseTable->clearContents();
    ui->licenseTable->setRowCount(0);
    ui->errorLabel->clear();

    if (result.status != MiniCloud::Server::LicenseManagerOperationStatus::Success)
    {
        ui->errorLabel->setText(result.errorMessage);
        refreshSelectionActions();
        return;
    }

    ui->licenseTable->setRowCount(result.licenses.size());
    int rowToSelect = -1;
    for (qsizetype row = 0; row < result.licenses.size(); ++row)
    {
        const MiniCloud::Server::LicenseView &license = result.licenses.at(row);
        auto *productKeyItem = new QTableWidgetItem(license.productKey);
        productKeyItem->setData(Qt::UserRole, license.productKey);
        ui->licenseTable->setItem(row, 0, productKeyItem);
        ui->licenseTable->setItem(row, 1, new QTableWidgetItem(license.deviceId));
        auto *statusItem = new QTableWidgetItem(license.enabled ? tr("Enabled") : tr("Disabled"));
        statusItem->setData(Qt::UserRole, license.enabled);
        ui->licenseTable->setItem(row, 2, statusItem);

        if (license.productKey == productKeyToSelect)
        {
            rowToSelect = static_cast<int>(row);
        }
    }

    if (rowToSelect >= 0)
    {
        ui->licenseTable->setCurrentCell(rowToSelect, 0);
        ui->licenseTable->selectRow(rowToSelect);
    }
    else
    {
        ui->licenseTable->clearSelection();
    }

    refreshSelectionActions();
}

void ServerWindow::refreshSelectionActions()
{
    const bool initialized = m_licenseManager->isInitialized();
    const bool hasSelection = !ui->licenseTable->selectedItems().isEmpty();
    const QTableWidgetItem *statusItem = hasSelection
                                             ? ui->licenseTable->item(ui->licenseTable->currentRow(), 2)
                                             : nullptr;
    const bool selectedLicenseIsEnabled = statusItem && statusItem->data(Qt::UserRole).toBool();

    ui->createLicenseButton->setEnabled(initialized);
    ui->refreshLicensesButton->setEnabled(initialized);
    ui->enableLicenseButton->setEnabled(initialized && hasSelection && !selectedLicenseIsEnabled);
    ui->disableLicenseButton->setEnabled(initialized && hasSelection && selectedLicenseIsEnabled);
}
