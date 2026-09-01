#include "ServerRequestDispatcher.h"

#include "ClientSession.h"
#include "authentication.h"
#include "errorresponse.h"
#include "licensemanager.h"

ServerRequestDispatcher::ServerRequestDispatcher(MiniCloud::Server::LicenseManager &licenseManager)
    : m_licenseManager(licenseManager) {}

void ServerRequestDispatcher::handleFrame(ClientSession &session, const MiniCloud::Protocol::ProtocolFrame &frame)
{
    if (frame.header.messageType == MiniCloud::Protocol::MessageType::FileChunk && !session.isAuthenticated())
    {
        const MiniCloud::Protocol::ErrorResponseData errorData{
            MiniCloud::Protocol::ErrorCode::AuthenticationFailed,
            QStringLiteral("Authentication is required to access file content.")};

        const MiniCloud::Protocol::ErrorResponseEncodeResult errorResponse = MiniCloud::Protocol::serializeErrorResponse(errorData);

        if (errorResponse.status == MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::ErrorResponse,
                frame.header.requestId,
                frame.header.taskId,
                errorResponse.payload);
        }
        return;
    }

    if (frame.header.messageType != MiniCloud::Protocol::MessageType::AuthenticateRequest)
    {
        return;
    }

    const MiniCloud::Protocol::AuthenticateRequestDecodeResult request = MiniCloud::Protocol::deserializeAuthenticateRequest(frame.payload);

    if (request.status != MiniCloud::Protocol::AuthenticateRequestDecodeResult::Status::Success)
    {
        const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("Authentication request is invalid.")};
        const MiniCloud::Protocol::ErrorResponseEncodeResult errorResponse = MiniCloud::Protocol::serializeErrorResponse(errorData);

        if (errorResponse.status == MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::ErrorResponse,
                frame.header.requestId,
                frame.header.taskId,
                errorResponse.payload);
        }
        return;
    }

    const MiniCloud::Server::AuthenticationResult authentication = m_licenseManager.authenticate(request.data.productKey, request.data.deviceId);

    if (authentication.operationStatus == MiniCloud::Server::LicenseManagerOperationStatus::Failed)
    {
        const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InternalServerError, QStringLiteral("Unable to process the authentication request.")};

        const MiniCloud::Protocol::ErrorResponseEncodeResult errorResponse = MiniCloud::Protocol::serializeErrorResponse(errorData);

        if (errorResponse.status == MiniCloud::Protocol::ErrorResponseEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::ErrorResponse,
                frame.header.requestId,
                frame.header.taskId,
                errorResponse.payload);
        }
        return;
    }

    if (authentication.operationStatus != MiniCloud::Server::LicenseManagerOperationStatus::Success)
    {
        return;
    }

    const MiniCloud::Protocol::AuthenticateResponseData responseData{authentication.authenticationStatus};
    const MiniCloud::Protocol::AuthenticationEncodeResult response = MiniCloud::Protocol::serializeAuthenticateResponse(responseData);
    if (response.status != MiniCloud::Protocol::AuthenticationEncodeResult::Status::Success)
    {
        return;
    }

    const bool sent = session.sendFrame(
        MiniCloud::Protocol::MessageType::AuthenticateResponse,
        frame.header.requestId,
        frame.header.taskId,
        response.payload);
    if (sent && authentication.authenticationStatus == MiniCloud::Protocol::AuthenticationStatus::Valid)
    {
        session.markAuthenticated();
    }
}
