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

void ServerRequestDispatcher::cancelActiveUpload()
{
    if (m_fileManager != nullptr)
    {
        m_fileManager->cancelUpload();
    }

    m_activeUploadRequestId.reset();
}

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

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::UploadStartRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::AuthenticationFailed, QStringLiteral("Authentication is required to start uploads.")};

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

        const MiniCloud::Protocol::UploadStartRequestDecodeResult request = MiniCloud::Protocol::deserializeUploadStartRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::UploadStartRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("Upload start request is invalid.")};

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

        const MiniCloud::Server::FileManagerUploadStartResult uploadResult =
            m_fileManager->beginUpload(request.data.destinationDirectoryPath, request.data.fileName, request.data.totalSizeBytes);

        if (uploadResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest, uploadResult.errorMessage};

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

        if (uploadResult.completed)
        {
            const MiniCloud::Protocol::FileOperationResponseData responseData{uploadResult.path};
            const MiniCloud::Protocol::FileProtocolEncodeResult response =
                MiniCloud::Protocol::serializeFileOperationResponse(responseData);

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

        m_activeUploadRequestId = frame.header.requestId;
        const MiniCloud::Protocol::UploadReadyResponseData responseData{uploadResult.path};

        const MiniCloud::Protocol::FileProtocolEncodeResult response = MiniCloud::Protocol::serializeUploadReadyResponse(responseData);

        if (response.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::UploadReadyResponse,
                frame.header.requestId,
                frame.header.taskId,
                response.payload);
        }
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::FileChunk)
    {
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

        if (!m_activeUploadRequestId.has_value() || frame.header.requestId != m_activeUploadRequestId.value())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest,
                QStringLiteral("File chunk request ID does not match the active upload.")};

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

        const MiniCloud::Protocol::FileChunkDecodeResult request = MiniCloud::Protocol::deserializeFileChunk(frame.payload);

        if (request.status != MiniCloud::Protocol::FileChunkDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("File chunk is invalid.")};

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

        const MiniCloud::Server::FileManagerUploadChunkResult chunkResult =
            m_fileManager->appendUploadChunk(request.data.offset, request.data.bytes);

        if (chunkResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            m_activeUploadRequestId.reset();
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest, chunkResult.errorMessage};

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

        if (!chunkResult.completed)
        {
            return;
        }

        m_activeUploadRequestId.reset();

        const MiniCloud::Protocol::FileOperationResponseData responseData{chunkResult.path};
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

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::DownloadRequest)
    {
        if (!session.isAuthenticated())
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::AuthenticationFailed, QStringLiteral("Authentication is required to download files.")};

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

        const MiniCloud::Protocol::DownloadRequestDecodeResult request = MiniCloud::Protocol::deserializeDownloadRequest(frame.payload);

        if (request.status != MiniCloud::Protocol::DownloadRequestDecodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InvalidRequest, QStringLiteral("Download request is invalid.")};

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

        const MiniCloud::Server::FileManagerDownloadStartResult downloadStartResult = m_fileManager->beginDownload(request.data.path);

        if (downloadStartResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
        {
            MiniCloud::Protocol::ErrorCode errorCode =
                MiniCloud::Protocol::ErrorCode::InternalServerError;

            switch (downloadStartResult.failureReason)
            {
            case MiniCloud::Server::FileManagerDownloadFailureReason::InvalidPath:
            case MiniCloud::Server::FileManagerDownloadFailureReason::TransferInProgress:
                errorCode = MiniCloud::Protocol::ErrorCode::InvalidRequest;
                break;
            case MiniCloud::Server::FileManagerDownloadFailureReason::NotFound:
                errorCode = MiniCloud::Protocol::ErrorCode::FileNotFound;
                break;
            case MiniCloud::Server::FileManagerDownloadFailureReason::IoFailure:
            case MiniCloud::Server::FileManagerDownloadFailureReason::None:
                break;
            }

            const MiniCloud::Protocol::ErrorResponseData errorData{
                errorCode, downloadStartResult.errorMessage};

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

        const MiniCloud::Protocol::DownloadStartResponseData downloadStartData{downloadStartResult.path, downloadStartResult.totalSizeBytes};

        const MiniCloud::Protocol::FileProtocolEncodeResult downloadStartResponse = MiniCloud::Protocol::serializeDownloadStartResponse(downloadStartData);

        if (downloadStartResponse.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            const MiniCloud::Protocol::ErrorResponseData errorData{
                MiniCloud::Protocol::ErrorCode::InternalServerError, QStringLiteral("Failed to serialize the download start response.")};

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

        if (!session.sendFrame(
                MiniCloud::Protocol::MessageType::DownloadStartResponse,
                frame.header.requestId,
                frame.header.taskId,
                downloadStartResponse.payload))
        {
            return;
        }

        while (!downloadStartResult.completed)
        {
            const MiniCloud::Server::FileManagerDownloadChunkResult chunkResult =
                m_fileManager->readNextDownloadChunk();

            if (chunkResult.status != MiniCloud::Server::FileManagerOperationStatus::Success)
            {
                const MiniCloud::Protocol::ErrorResponseData errorData{
                    MiniCloud::Protocol::ErrorCode::InternalServerError, chunkResult.errorMessage};

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

            const MiniCloud::Protocol::FileProtocolEncodeResult chunkResponse =
                MiniCloud::Protocol::serializeFileChunk({chunkResult.offset, chunkResult.data});

            if (chunkResponse.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
            {
                const MiniCloud::Protocol::ErrorResponseData errorData{
                    MiniCloud::Protocol::ErrorCode::InternalServerError, QStringLiteral("Failed to serialize a download chunk.")};

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

            if (!session.sendFrame(
                    MiniCloud::Protocol::MessageType::FileChunk,
                    frame.header.requestId,
                    frame.header.taskId,
                    chunkResponse.payload))
            {
                return;
            }

            if (chunkResult.completed)
            {
                break;
            }
        }

        const MiniCloud::Protocol::FileOperationResponseData completionData{downloadStartResult.path};

        const MiniCloud::Protocol::FileProtocolEncodeResult completionResponse = MiniCloud::Protocol::serializeFileOperationResponse(completionData);

        if (completionResponse.status == MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
        {
            session.sendFrame(
                MiniCloud::Protocol::MessageType::FileOperationResponse,
                frame.header.requestId,
                frame.header.taskId,
                completionResponse.payload);
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
