#include "applicationcontroller.h"

#include <QDir>
#include <QFileInfo>

#include "authentication.h"
#include "protocolconstants.h"
#include "requesttypes.h"

ApplicationController::ApplicationController(QObject *parent)
    : ApplicationController(50000, parent) {}

ApplicationController::ApplicationController(int requestTimeoutMs, QObject *parent)
    : QObject(parent), m_networkClient(), m_requestDispatcher(&m_networkClient, requestTimeoutMs)
{
    connect(
        &m_requestDispatcher,
        &RequestDispatcher::responseReceived,
        this,
        &ApplicationController::onResponseReceived);

    connect(
        &m_requestDispatcher,
        &RequestDispatcher::errorResponseReceived,
        this,
        &ApplicationController::onErrorResponseReceived);

    connect(
        &m_requestDispatcher,
        &RequestDispatcher::requestFailed,
        this,
        &ApplicationController::onRequestFailed);

    connect(
        &m_networkClient,
        &NetworkClient::disconnected,
        this,
        &ApplicationController::onDisconnected);

    connect(
        &m_networkClient,
        &NetworkClient::frameReceived,
        this,
        &ApplicationController::onUploadCompletionFrame);

    connect(
        &m_networkClient,
        &NetworkClient::frameReceived,
        this,
        &ApplicationController::onDownloadFrameReceived);

    connect(
        &m_networkClient,
        &NetworkClient::connected,
        this,
        [this]()
        {
            emit connectionStateChanged(true);
        });

    connect(
        &m_networkClient,
        &NetworkClient::connectionError,
        this,
        [this](QAbstractSocket::SocketError, const QString &message)
        {
            setAccessState(MiniCloud::Client::ClientAccessState::Locked);
            emit connectionFailed(message);
        });
}

MiniCloud::Client::ClientAccessState ApplicationController::accessState() const noexcept
{
    return m_accessState;
}

bool ApplicationController::isFeatureAccessAllowed() const noexcept
{
    return m_accessState == MiniCloud::Client::ClientAccessState::Active;
}

bool ApplicationController::connectToServer(const QString &hostname, quint16 port)
{
    return m_networkClient.connectToServer(hostname, port);
}

void ApplicationController::disconnectFromServer()
{
    m_networkClient.disconnectFromServer();
}

bool ApplicationController::isConnected() const
{
    return m_networkClient.isConnected();
}

MiniCloud::Client::RequestSendResult ApplicationController::activate(const QString &licenseKey, const QString &deviceId)
{
    if (m_accessState == MiniCloud::Client::ClientAccessState::Authenticating)
    {
        MiniCloud::Client::RequestSendResult result;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidRequest;
        return result;
    }

    if (licenseKey.trimmed().isEmpty() || deviceId.trimmed().isEmpty())
    {
        MiniCloud::Client::RequestSendResult result;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidRequest;
        return result;
    }

    const MiniCloud::Protocol::AuthenticateRequestData requestData{licenseKey, deviceId};

    const auto encoded = MiniCloud::Protocol::serializeAuthenticateRequest(requestData);

    if (encoded.status != MiniCloud::Protocol::AuthenticationEncodeResult::Status::Success)
    {
        m_activationRequestId = 0;
        setAccessState(MiniCloud::Client::ClientAccessState::Locked);
        MiniCloud::Client::RequestSendResult result;
        result.errorCode = MiniCloud::Client::RequestDispatchError::InvalidRequest;
        return result;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(MiniCloud::Protocol::MessageType::AuthenticateRequest,
                                                                                        0, // task id
                                                                                        encoded.payload,
                                                                                        MiniCloud::Client::RequestDestination::License);

    if (result.status == MiniCloud::Client::RequestSendStatus::Accepted)
    {
        m_activationRequestId = result.requestId;
        setAccessState(MiniCloud::Client::ClientAccessState::Authenticating);
    }
    else
    {
        m_activationRequestId = 0;
    }

    return result;
}

bool ApplicationController::requestBrowse(const QString &path)
{
    if (!isFeatureAccessAllowed())
    {
        emit browseFailed(QStringLiteral("Session must be authenticated before browsing files."));
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded = MiniCloud::Protocol::serializeBrowseRequest({path});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::BrowseRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    return result.status == MiniCloud::Client::RequestSendStatus::Accepted;
}

bool ApplicationController::requestSearch(const QString &path, const QString &query)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded = MiniCloud::Protocol::serializeSearchRequest({path, query});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::SearchRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    return result.status == MiniCloud::Client::RequestSendStatus::Accepted;
}

bool ApplicationController::requestCreateDirectory(const QString &parentPath, const QString &name)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    if (parentPath.trimmed().isEmpty() || name.trimmed().isEmpty())
    {
        emit fileOperationFailed(QStringLiteral("Parent path and directory name must not be blank."));
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded =
        MiniCloud::Protocol::serializeCreateDirectoryRequest({parentPath, name});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::CreateDirectoryRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    return result.status == MiniCloud::Client::RequestSendStatus::Accepted;
}

bool ApplicationController::requestRename(const QString &path, const QString &newName)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded =
        MiniCloud::Protocol::serializeRenameRequest({path, newName});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::RenameRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    return result.status == MiniCloud::Client::RequestSendStatus::Accepted;
}

bool ApplicationController::requestMove(const QString &sourcePath, const QString &destinationDirectoryPath)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded =
        MiniCloud::Protocol::serializeMoveRequest({sourcePath, destinationDirectoryPath});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::MoveRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    return result.status == MiniCloud::Client::RequestSendStatus::Accepted;
}

bool ApplicationController::requestDelete(const QString &path)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded =
        MiniCloud::Protocol::serializeDeleteRequest({path});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::DeleteRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    return result.status == MiniCloud::Client::RequestSendStatus::Accepted;
}

bool ApplicationController::requestUpload(const QString &localFilePath, const QString &destinationDirectoryPath)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    if (m_activeUploadRequestId != 0)
    {
        emit fileOperationFailed(QStringLiteral("An upload is already in progress."));
        return false;
    }

    if (m_activeDownloadRequestId != 0)
    {
        emit fileOperationFailed(QStringLiteral("A download is already in progress."));
        return false;
    }

    const QFileInfo localFileInfo(localFilePath);
    if (!localFileInfo.isFile())
    {
        emit fileOperationFailed(QStringLiteral("The selected upload file does not exist."));
        return false;
    }

    m_activeUploadFile.setFileName(localFilePath);

    if (!m_activeUploadFile.open(QIODevice::ReadOnly))
    {
        emit fileOperationFailed(QStringLiteral("The selected upload file cannot be opened."));
        return false;
    }

    const quint64 totalSizeBytes = static_cast<quint64>(m_activeUploadFile.size());
    const MiniCloud::Protocol::FileProtocolEncodeResult encoded =
        MiniCloud::Protocol::serializeUploadStartRequest({destinationDirectoryPath, localFileInfo.fileName(), totalSizeBytes});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        resetActiveUpload();
        return false;
    }

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::UploadStartRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    if (result.status != MiniCloud::Client::RequestSendStatus::Accepted)
    {
        resetActiveUpload();
        return false;
    }

    m_activeUploadRequestId = result.requestId;
    m_activeUploadTaskId = 0;
    m_activeUploadTotalSizeBytes = totalSizeBytes;
    m_activeUploadBytesTransferred = 0;
    m_uploadAwaitingReady = true;
    return true;
}

bool ApplicationController::requestDownload(const QString &remotePath, const QString &localFilePath)
{
    if (!isFeatureAccessAllowed())
    {
        return false;
    }

    if (m_activeDownloadRequestId != 0)
    {
        emit fileOperationFailed(QStringLiteral("A download is already in progress."));
        return false;
    }

    if (m_activeUploadRequestId != 0)
    {
        emit fileOperationFailed(QStringLiteral("An upload is already in progress."));
        return false;
    }

    const MiniCloud::Protocol::FileProtocolEncodeResult encoded = MiniCloud::Protocol::serializeDownloadRequest({remotePath});

    if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success)
    {
        return false;
    }

    const QFileInfo localTargetInfo(localFilePath);
    if (localTargetInfo.fileName().isEmpty() || localTargetInfo.exists() || !localTargetInfo.absoluteDir().exists())
    {
        emit fileOperationFailed(QStringLiteral("The download target path is unavailable."));
        return false;
    }

    auto downloadFile = std::make_unique<QSaveFile>(localTargetInfo.absoluteFilePath());
    if (!downloadFile->open(QIODevice::WriteOnly))
    {
        emit fileOperationFailed(QStringLiteral("The download target file cannot be opened."));
        return false;
    }

    m_activeDownloadFile = std::move(downloadFile);

    const MiniCloud::Client::RequestSendResult result = m_requestDispatcher.sendRequest(
        MiniCloud::Protocol::MessageType::DownloadRequest,
        0,
        encoded.payload,
        MiniCloud::Client::RequestDestination::File);

    if (result.status != MiniCloud::Client::RequestSendStatus::Accepted)
    {
        resetActiveDownload();
        return false;
    }

    m_activeDownloadRequestId = result.requestId;
    m_activeDownloadTaskId = 0;
    m_activeDownloadTotalSizeBytes = 0;
    m_activeDownloadBytesTransferred = 0;
    m_downloadAwaitingStart = true;
    return true;
}

void ApplicationController::onResponseReceived(MiniCloud::Client::RequestDestination destination, const MiniCloud::Protocol::ProtocolFrame &frame)
{
    if (destination == MiniCloud::Client::RequestDestination::File && frame.header.messageType == MiniCloud::Protocol::MessageType::DownloadStartResponse)
    {
        if (frame.header.requestId != m_activeDownloadRequestId || !m_downloadAwaitingStart)
        {
            return;
        }

        const MiniCloud::Protocol::DownloadStartResponseDecodeResult decoded = MiniCloud::Protocol::deserializeDownloadStartResponse(frame.payload);

        if (decoded.status != MiniCloud::Protocol::DownloadStartResponseDecodeResult::Status::Success)
        {
            resetActiveDownload();
            emit fileOperationFailed(QStringLiteral("The download-start response is invalid."));
            return;
        }

        m_activeDownloadTotalSizeBytes = decoded.data.totalSizeBytes;
        m_activeDownloadBytesTransferred = 0;
        m_downloadAwaitingStart = false;

        if (m_activeDownloadTotalSizeBytes == 0)
        {
            emit downloadProgress(0, 0);
        }
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File && frame.header.messageType == MiniCloud::Protocol::MessageType::UploadReadyResponse)
    {
        if (frame.header.requestId != m_activeUploadRequestId || !m_uploadAwaitingReady)
        {
            return;
        }

        const MiniCloud::Protocol::UploadReadyResponseDecodeResult decoded =
            MiniCloud::Protocol::deserializeUploadReadyResponse(frame.payload);

        if (decoded.status != MiniCloud::Protocol::UploadReadyResponseDecodeResult::Status::Success)
        {
            resetActiveUpload();
            emit fileOperationFailed(QStringLiteral("The upload-ready response is invalid."));
            return;
        }

        m_uploadAwaitingReady = false;
        sendUploadChunks();
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File && frame.header.messageType == MiniCloud::Protocol::MessageType::FileOperationResponse)
    {
        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(frame.payload);

        if (frame.header.requestId == m_activeUploadRequestId)
        {
            if (m_uploadAwaitingReady && m_activeUploadTotalSizeBytes == 0 && decoded.status == MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success)
            {
                resetActiveUpload();
                emit uploadProgress(0, 0);
                emit fileOperationCompleted(decoded.data.path);
                return;
            }

            resetActiveUpload();
            emit fileOperationFailed(QStringLiteral("The upload completion response is invalid."));
            return;
        }

        if (decoded.status == MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success)
        {
            emit fileOperationCompleted(decoded.data.path);
        }
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File && frame.header.messageType == MiniCloud::Protocol::MessageType::BrowseResponse)
    {
        const MiniCloud::Protocol::BrowseResponseDecodeResult decoded =
            MiniCloud::Protocol::deserializeBrowseResponse(frame.payload);

        if (decoded.status == MiniCloud::Protocol::BrowseResponseDecodeResult::Status::Success)
        {
            emit browseReceived(decoded.data.path, decoded.data.entries);
        }
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File && frame.header.messageType == MiniCloud::Protocol::MessageType::SearchResponse)
    {
        const MiniCloud::Protocol::SearchResponseDecodeResult decoded =
            MiniCloud::Protocol::deserializeSearchResponse(frame.payload);

        if (decoded.status == MiniCloud::Protocol::SearchResponseDecodeResult::Status::Success)
        {
            emit searchReceived(decoded.data.path, decoded.data.entries);
        }
        return;
    }

    if (destination != MiniCloud::Client::RequestDestination::License || frame.header.requestId != m_activationRequestId || frame.header.messageType != MiniCloud::Protocol::MessageType::AuthenticateResponse)
    {
        return;
    }

    const MiniCloud::Protocol::AuthenticateResponseDecodeResult decoded = MiniCloud::Protocol::deserializeAuthenticateResponse(frame.payload);
    if (decoded.status != MiniCloud::Protocol::AuthenticateResponseDecodeResult::Status::Success)
    {
        m_activationRequestId = 0;
        setAccessState(MiniCloud::Client::ClientAccessState::Locked);
        emit activationFailed(MiniCloud::Client::RequestDispatchError::InvalidResponsePayload);
        return;
    }

    if (decoded.data.status == MiniCloud::Protocol::AuthenticationStatus::Valid)
    {
        m_activationRequestId = 0;
        setAccessState(MiniCloud::Client::ClientAccessState::Active);
        return;
    }

    m_activationRequestId = 0;
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit activationRejected(decoded.data.status);
}

void ApplicationController::onErrorResponseReceived(
    MiniCloud::Client::RequestDestination destination,
    MiniCloud::Protocol::RequestId requestId,
    MiniCloud::Protocol::TaskId taskId,
    const MiniCloud::Protocol::ErrorResponseData &error)
{
    Q_UNUSED(taskId);

    if (destination == MiniCloud::Client::RequestDestination::File && requestId == m_activeUploadRequestId)
    {
        resetActiveUpload();
        emit fileOperationFailed(error.message);
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File && requestId == m_activeDownloadRequestId)
    {
        resetActiveDownload();
        emit fileOperationFailed(error.message);
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File)
    {
        emit browseFailed(error.message);
        return;
    }

    if (destination != MiniCloud::Client::RequestDestination::License || requestId != m_activationRequestId)
    {
        return;
    }

    m_activationRequestId = 0;
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit activationError(error);
}

void ApplicationController::onRequestFailed(
    MiniCloud::Client::RequestDestination destination,
    MiniCloud::Protocol::RequestId requestId,
    MiniCloud::Protocol::TaskId taskId,
    MiniCloud::Client::RequestDispatchError error)
{
    Q_UNUSED(taskId);

    if (destination == MiniCloud::Client::RequestDestination::File && requestId == m_activeUploadRequestId)
    {
        resetActiveUpload();
        emit fileOperationFailed(QStringLiteral("The upload request failed."));
        return;
    }

    if (destination == MiniCloud::Client::RequestDestination::File && requestId == m_activeDownloadRequestId)
    {
        resetActiveDownload();
        emit fileOperationFailed(QStringLiteral("The download request failed."));
        return;
    }

    if (destination != MiniCloud::Client::RequestDestination::License || requestId != m_activationRequestId)
    {
        return;
    }

    m_activationRequestId = 0;
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);
    emit activationFailed(error);
}

void ApplicationController::onDisconnected()
{
    const bool hadActiveUpload = m_activeUploadRequestId != 0;
    const bool hadActiveDownload = m_activeDownloadRequestId != 0;

    resetActiveUpload();
    resetActiveDownload();
    setAccessState(MiniCloud::Client::ClientAccessState::Locked);

    if (hadActiveUpload)
    {
        emit fileOperationFailed(QStringLiteral("The upload connection was lost."));
    }

    if (hadActiveDownload)
    {
        emit fileOperationFailed(QStringLiteral("The download connection was lost."));
    }

    emit connectionStateChanged(false);
}

void ApplicationController::onUploadCompletionFrame(const MiniCloud::Protocol::ProtocolFrame &frame)
{
    if (frame.header.messageType != MiniCloud::Protocol::MessageType::FileOperationResponse || frame.header.requestId != m_activeUploadRequestId || m_uploadAwaitingReady || m_activeUploadBytesTransferred != m_activeUploadTotalSizeBytes)
    {
        return;
    }

    const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded =
        MiniCloud::Protocol::deserializeFileOperationResponse(frame.payload);

    if (decoded.status != MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success)
    {
        resetActiveUpload();
        emit fileOperationFailed(QStringLiteral("The upload completion response is invalid."));
        return;
    }

    resetActiveUpload();
    emit fileOperationCompleted(decoded.data.path);
}

void ApplicationController::onDownloadFrameReceived(const MiniCloud::Protocol::ProtocolFrame &frame)
{
    if (frame.header.requestId != m_activeDownloadRequestId || m_downloadAwaitingStart)
    {
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::ErrorResponse)
    {
        const MiniCloud::Protocol::ErrorResponseDecodeResult decoded =
            MiniCloud::Protocol::deserializeErrorResponse(frame.payload);
        const QString message = decoded.status == MiniCloud::Protocol::ErrorResponseDecodeResult::Status::Success
                                    ? decoded.data.message
                                    : QStringLiteral("The download error response is invalid.");

        resetActiveDownload();
        emit fileOperationFailed(message);
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::FileChunk)
    {
        const MiniCloud::Protocol::FileChunkDecodeResult decoded = MiniCloud::Protocol::deserializeFileChunk(frame.payload);
        const quint64 chunkSizeBytes = static_cast<quint64>(decoded.data.bytes.size());

        if (decoded.status != MiniCloud::Protocol::FileChunkDecodeResult::Status::Success || !m_activeDownloadFile || decoded.data.offset != m_activeDownloadBytesTransferred || chunkSizeBytes > m_activeDownloadTotalSizeBytes - m_activeDownloadBytesTransferred)
        {
            resetActiveDownload();
            emit fileOperationFailed(QStringLiteral("The download chunk is invalid."));
            return;
        }

        const qint64 writtenBytes = m_activeDownloadFile->write(decoded.data.bytes);
        if (writtenBytes != static_cast<qint64>(decoded.data.bytes.size()))
        {
            resetActiveDownload();
            emit fileOperationFailed(QStringLiteral("The download target file could not be written."));
            return;
        }

        m_activeDownloadBytesTransferred += chunkSizeBytes;
        emit downloadProgress(m_activeDownloadBytesTransferred, m_activeDownloadTotalSizeBytes);
        return;
    }

    if (frame.header.messageType == MiniCloud::Protocol::MessageType::FileOperationResponse)
    {
        const MiniCloud::Protocol::FileOperationResponseDecodeResult decoded = MiniCloud::Protocol::deserializeFileOperationResponse(frame.payload);

        if (!m_activeDownloadFile || m_activeDownloadBytesTransferred != m_activeDownloadTotalSizeBytes || decoded.status != MiniCloud::Protocol::FileOperationResponseDecodeResult::Status::Success || !m_activeDownloadFile->commit())
        {
            resetActiveDownload();
            emit fileOperationFailed(QStringLiteral("The download completion response is invalid."));
            return;
        }

        resetActiveDownload();
        emit fileOperationCompleted(decoded.data.path);
    }
}

void ApplicationController::sendUploadChunks()
{
    while (m_activeUploadBytesTransferred < m_activeUploadTotalSizeBytes)
    {
        const quint64 remainingBytes =
            m_activeUploadTotalSizeBytes - m_activeUploadBytesTransferred;
        const qint64 maximumBytesToRead = static_cast<qint64>(qMin(
            remainingBytes,
            static_cast<quint64>(MiniCloud::Protocol::protocolMaxFileChunkDataBytes)));
        const QByteArray bytes = m_activeUploadFile.read(maximumBytesToRead);

        if (bytes.isEmpty())
        {
            resetActiveUpload();
            emit fileOperationFailed(QStringLiteral("The upload file could not be read."));
            return;
        }

        const MiniCloud::Protocol::FileProtocolEncodeResult encoded =
            MiniCloud::Protocol::serializeFileChunk(
                {m_activeUploadBytesTransferred, bytes});

        if (encoded.status != MiniCloud::Protocol::FileProtocolEncodeResult::Status::Success || !m_networkClient.sendFrame(
                                                                                                    MiniCloud::Protocol::MessageType::FileChunk,
                                                                                                    m_activeUploadRequestId,
                                                                                                    m_activeUploadTaskId,
                                                                                                    encoded.payload))
        {
            resetActiveUpload();
            emit fileOperationFailed(QStringLiteral("The upload chunk could not be sent."));
            return;
        }

        m_activeUploadBytesTransferred += static_cast<quint64>(bytes.size());
        emit uploadProgress(
            m_activeUploadBytesTransferred, m_activeUploadTotalSizeBytes);
    }
}

void ApplicationController::resetActiveUpload()
{
    if (m_activeUploadFile.isOpen())
    {
        m_activeUploadFile.close();
    }

    m_activeUploadRequestId = 0;
    m_activeUploadTaskId = 0;
    m_activeUploadTotalSizeBytes = 0;
    m_activeUploadBytesTransferred = 0;
    m_uploadAwaitingReady = false;
}

void ApplicationController::resetActiveDownload()
{
    m_activeDownloadFile.reset();
    m_activeDownloadRequestId = 0;
    m_activeDownloadTaskId = 0;
    m_activeDownloadTotalSizeBytes = 0;
    m_activeDownloadBytesTransferred = 0;
    m_downloadAwaitingStart = false;
}

void ApplicationController::setAccessState(MiniCloud::Client::ClientAccessState state)
{
    if (m_accessState == state)
        return;
    m_accessState = state;
    emit accessStateChanged(state);
}
