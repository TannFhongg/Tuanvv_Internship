#pragma once
#include <functional>
#include <QtGlobal>
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

    struct LicenseView
    {
        QString productKey;
        QString deviceId;
        bool enabled{false};
    };

    struct LicenseListResult
    {
        LicenseManagerOperationStatus status{
            LicenseManagerOperationStatus::Failed};

        QList<LicenseView> licenses;
        QString errorMessage;
    };

    class LicenseManager
    {
    public:
        using EntropySource = std::function<quint64()>;

        explicit LicenseManager(
            QString repositoryFilePath,
            EntropySource entropySource = {});

        LicenseManagerResult initialize();

        bool isInitialized() const noexcept;

        MiniCloud::Server::AuthenticationResult authenticate(const QString &productKey, const QString &deviceId);

        MiniCloud::Server::CreateLicenseResult createLicense();

        LicenseManagerResult disableLicense(const QString &productKey);
        LicenseManagerResult enableLicense(const QString &productKey);
        LicenseListResult listLicenses() const;

    private:
        LicenseRepository m_repository;
        EntropySource m_entropySource;
        bool m_initialized{false};
        QString generateCandidateProductKey() const;

        LicenseManagerResult setLicenseEnabled(const QString &productKey, bool enabled);
    };
}
