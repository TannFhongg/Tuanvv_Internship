#include "ServerRequestDispatcher.h"

#include "ClientSession.h"
#include "authentication.h"
#include "errorresponse.h"
#include "filemanager.h"
#include "fileprotocol.h"
#include "licensemanager.h"

ServerRequestDispatcher::ServerRequestDispatcher(MiniCloud::Server::LicenseManager &licenseManager)
    : m_licenseManager(licenseManager) {}

ServerRequestDispatcher::ServerRequestDispatcher(MiniCloud::Server::LicenseManager &licenseManager, MiniCloud::Server::FileManager &fileManager)
    : m_licenseManager(licenseManager), m_fileManager(&fileManager) {}

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

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::BrowseRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::AuthenticationFailed, QStringLiteral("Authentication is required to browse files.")};

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

        if (m_fileManager == nullptr)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InternalServerError, QStringLiteral("File manager is unavailable.")};

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

        const MiniCloud::Protocol::BrowseRequestDecodeResult request = MiniCloud::Protocol::deserializeBrowseRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::BrowseRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("Browse request is invalid.")};

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

        const MiniCloud::Server::FileManagerBrowseResult browseResult = m_fileManager->browse(request.data.path);

        if (browseResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            MiniCloud::Protocol::ErrorCode errorCode = MiniCloud::Protocol::ErrorCode::InternalServerError;

            switch (browseResult.failureReason)
            {
            case MiniCloud::Server::FileManagerBrowseFailureReason::InvalidPath:
                errorCode = MiniCloud::Protocol::ErrorCode::InvalidRequest;
                break;
            case MiniCloud::Server::FileManagerBrowseFailureReason::NotFound:
                errorCode = MiniCloud::Protocol::ErrorCode::FileNotFound;
                break;
            case MiniCloud::Server::FileManagerBrowseFailureReason::IoFailure:
            case MiniCloud::Server::FileManagerBrowseFailureReason::None:
                break;
            }

            const MiniCloud::Protocol::ErrorResponseData errorData{errorCode, browseResult.errorMessage};

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

        const MiniCloud::Protocol::BrowseResponseData responseData{request.data.path, browseResult.entries};

        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeBrowseResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::BrowseResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
        }
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::SearchRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::AuthenticationFailed,
                QStringLiteral("Authentication is required to search files.")};
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

        if (m_fileManager == nullptr)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InternalServerError,
                QStringLiteral("File manager is unavailable.")};
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

        const MiniCloud::Protocol::SearchRequestDecodeResult request = MiniCloud::Protocol::deserializeSearchRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::SearchRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest,
                QStringLiteral("Search request is invalid.")};
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

        const MiniCloud::Server::FileManagerBrowseResult searchResult = m_fileManager->search(request.data.path, request.data.query);

        if (searchResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            MiniCloud::Protocol::ErrorCode errorCode = MiniCloud::Protocol::ErrorCode::InternalServerError;

            switch (searchResult.failureReason)
            {
            case MiniCloud::Server::FileManagerBrowseFailureReason::InvalidPath:
                errorCode = MiniCloud::Protocol::ErrorCode::InvalidRequest;
                break;
            case MiniCloud::Server::FileManagerBrowseFailureReason::NotFound:
                errorCode = MiniCloud::Protocol::ErrorCode::FileNotFound;
                break;
            case MiniCloud::Server::FileManagerBrowseFailureReason::IoFailure:
            case MiniCloud::Server::FileManagerBrowseFailureReason::None:
                break;
            }

            const MiniCloud::Protocol::ErrorResponseData errorData{errorCode, searchResult.errorMessage};
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

        const MiniCloud::Protocol::SearchResponseData responseData{request.data.path, searchResult.entries};
        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeSearchResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::SearchResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
        }
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::CreateDirectoryRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::AuthenticationFailed,
                QStringLiteral("Authentication is required to create directories.")};
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

        if (m_fileManager == nullptr)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InternalServerError,
                QStringLiteral("File manager is unavailable.")};
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

        const MiniCloud::Protocol::CreateDirectoryRequestDecodeResult request = MiniCloud::Protocol::deserializeCreateDirectoryRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::CreateDirectoryRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest,
                QStringLiteral("Create directory request is invalid.")};
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

        const MiniCloud::Server::FileManagerOperationResult createDirectoryResult = m_fileManager->createDirectory(request.data.parentPath, request.data.name);

        if (createDirectoryResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InvalidRequest, createDirectoryResult.errorMessage};

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

        const MiniCloud::Protocol::FileOperationResponseData responseData{createDirectoryResult.path};
        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeFileOperationResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::FileOperationResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
        }
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::RenameRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::AuthenticationFailed,
                QStringLiteral("Authentication is required to rename entries.")};
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

        if (m_fileManager == nullptr)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InternalServerError,
                QStringLiteral("File manager is unavailable.")};
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

        const MiniCloud::Protocol::RenameRequestDecodeResult request = MiniCloud::Protocol::deserializeRenameRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::RenameRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest,
                QStringLiteral("Rename request is invalid.")};
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

        const MiniCloud::Server::FileManagerOperationResult renameResult = m_fileManager->rename(
            request.data.path,
            request.data.newName);

        if (renameResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest,
                renameResult.errorMessage};
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

        const MiniCloud::Protocol::FileOperationResponseData responseData{renameResult.path};
        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeFileOperationResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::FileOperationResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
        }
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::MoveRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::AuthenticationFailed, QStringLiteral("Authentication is required to move entries.")};
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

        if (m_fileManager == nullptr)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InternalServerError, QStringLiteral("File manager is unavailable.")};

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

        const MiniCloud::Protocol::MoveRequestDecodeResult request = MiniCloud::Protocol::deserializeMoveRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::MoveRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("Move request is invalid.")};

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

        const MiniCloud::Server::FileManagerOperationResult moveResult = m_fileManager->move(
            request.data.sourcePath,
            request.data.destinationDirectoryPath);

        if (moveResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InvalidRequest, moveResult.errorMessage};
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

        const MiniCloud::Protocol::FileOperationResponseData responseData{moveResult.path};
        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeFileOperationResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::FileOperationResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
        }
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::DeleteRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::AuthenticationFailed, QStringLiteral("Authentication is required to delete entries.")};

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

        if (m_fileManager == nullptr)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InternalServerError, QStringLiteral("File manager is unavailable.")};

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

        const MiniCloud::Protocol::DeleteRequestDecodeResult request = MiniCloud::Protocol::deserializeDeleteRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::DeleteRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("Delete request is invalid.")};

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

        const MiniCloud::Server::FileManagerOperationResult removeResult = m_fileManager->remove(request.data.path);

        if (removeResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest, removeResult.errorMessage};

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

        const MiniCloud::Protocol::FileOperationResponseData responseData{removeResult.path};
        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeFileOperationResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::FileOperationResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
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
