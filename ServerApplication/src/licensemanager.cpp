#include "licensemanager.h"
#include <utility>
#include "licenserepository.h"

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

}
