#pragma once
#include <QString>
#include "licenserepository.h"
#include "authentication.h"

namespace MiniCloud::Server
{
    enum class LicenseManagerOperationStatus
    {
        Success,
        Failed
    };

    struct LicenseManagerResult
    {
        LicenseManagerOperationStatus status{LicenseManagerOperationStatus::Failed};

        QString errorMessage;
    };

    struct AuthenticationResult
    {
        LicenseManagerOperationStatus operationStatus{LicenseManagerOperationStatus::Failed};
        MiniCloud::Protocol::AuthenticationStatus authenticationStatus{MiniCloud::Protocol::AuthenticationStatus::Invalid};
        QString errorMessage;
    };

    struct CreateLicenseResult
    {
        LicenseManagerOperationStatus operationStatus{LicenseManagerOperationStatus::Failed};
        QString productKey;
        QString errorMessage;
    };

    class LicenseManager
    {
    public:
        explicit LicenseManager(QString repositoryFilePath);

        LicenseManagerResult initialize();

        bool isInitialized() const noexcept;

        MiniCloud::Server::AuthenticationResult authenticate(const QString &productKey, const QString &deviceId);

        MiniCloud::Server::CreateLicenseResult createLicense();

    private:
        LicenseRepository m_repository;
        bool m_initialized{false};
        QString generateCandidateProductKey() const;
    };
}