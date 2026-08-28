#include "licensemanager.h"
#include <utility>
#include "licenserepository.h"
#include <QRandomGenerator>
namespace MiniCloud::Server
{
    LicenseManager::LicenseManager(QString repositoryFilePath)
        : m_repository(std::move(repositoryFilePath))
    {
    }

    bool LicenseManager::isInitialized() const noexcept
    {
        return m_initialized;
    }

    LicenseManagerResult LicenseManager::initialize()
    {

        LicenseManagerResult result;

        auto repositoryResult = m_repository.load();

        if (repositoryResult.status == LicenseRepositoryStatus::Failed)
        {
            m_initialized = false;
            result.errorMessage = repositoryResult.errorMessage;
            return result;
        }

        m_initialized = true;
        result.status = LicenseManagerOperationStatus::Success;
        result.errorMessage.clear();
        return result;
    }

    MiniCloud::Server::AuthenticationResult LicenseManager::authenticate(const QString &productKey, const QString &deviceId)
    {

        MiniCloud::Server::AuthenticationResult result;
        if (!m_initialized)
        {
            result.errorMessage = "LicenseManager is not initialized.";
            return result;
        }
        if (productKey.trimmed().isEmpty())
        {
            result.errorMessage = "Product key is required.";
            return result;
        }
        if (deviceId.trimmed().isEmpty())
        {
            result.errorMessage = "Device ID is required.";
            return result;
        }

        const std::optional<LicenseRecord> record = m_repository.findByProductKey(productKey);
        if (!record.has_value())
        {
            result.operationStatus = LicenseManagerOperationStatus::Success;
            result.authenticationStatus = MiniCloud::Protocol::AuthenticationStatus::InvalidKey;
            result.errorMessage.clear();
            return result;
        }

        if (record->enabled == false)
        {
            result.operationStatus = LicenseManagerOperationStatus::Success;
            result.authenticationStatus = MiniCloud::Protocol::AuthenticationStatus::Disabled;
            return result;
        }
        if (record->deviceId.isEmpty())
        {
            LicenseRecord updatedRecord = record.value();
            updatedRecord.deviceId = deviceId;

            const LicenseRepositoryResult updateResult = m_repository.update(updatedRecord);
            if (updateResult.status == LicenseRepositoryStatus::Failed)
            {
                result.errorMessage = updateResult.errorMessage;
                return result;
            }

            result.operationStatus = LicenseManagerOperationStatus::Success;
            result.authenticationStatus = MiniCloud::Protocol::AuthenticationStatus::Valid;
            return result;
        }

        if (record->deviceId == deviceId)
        {
            result.operationStatus = LicenseManagerOperationStatus::Success;
            result.authenticationStatus = MiniCloud::Protocol::AuthenticationStatus::Valid;
            return result;
        }

        if (record->deviceId != deviceId)
        {
            result.operationStatus = LicenseManagerOperationStatus::Success;
            result.authenticationStatus = MiniCloud::Protocol::AuthenticationStatus::DeviceMismatch;
            return result;
        }

        result.errorMessage = "Authentication is not implemented.";
        return result;
    }

    MiniCloud::Server::CreateLicenseResult LicenseManager::createLicense()
    {
        MiniCloud::Server::CreateLicenseResult result;

        if (!m_initialized)
        {
            result.errorMessage = "LicenseManager is not initialized.";
            return result;
        }

        constexpr int maxAttempts = 100;

        for (int attempt = 0; attempt < maxAttempts; ++attempt)
        {
            const QString candidate = generateCandidateProductKey();

            if (m_repository.findByProductKey(candidate).has_value())
            {
                continue;
            }

            const LicenseRecord record{
                candidate,
                QString(),
                true};

            const LicenseRepositoryResult insertResult = m_repository.insert(record);
            if (insertResult.status == LicenseRepositoryStatus::Failed)
            {
                result.errorMessage = insertResult.errorMessage;
                return result;
            }

            result.operationStatus = LicenseManagerOperationStatus::Success;
            result.productKey = candidate;
            return result;
        }

        result.errorMessage = "Unable to generate a unique Product Key.";
        return result;
    }

    QString LicenseManager::generateCandidateProductKey() const
    {
        const quint64 randomValue = QRandomGenerator::system()->generate64();

        const QString hex = QStringLiteral("%1").arg(randomValue, 16, 16, QLatin1Char('0')).toUpper();
        return hex;
    }
}
