#pragma once

#include <memory>

#include <QFile>
#include <QMetaType>
#include <QObject>
#include <QList>
#include <QSaveFile>
#include "NetworkClient.h"
#include "requestdispatcher.h"
#include "requesttypes.h"
#include "authentication.h"
#include "errorresponse.h"
#include "fileprotocol.h"

namespace MiniCloud::Client
{
    enum class ClientAccessState
    {
        Locked,
        Authenticating,
        Active
    };
}

class ApplicationController : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationController(QObject *parent = nullptr);
    ApplicationController(int requestTimeoutMs, QObject *parent = nullptr);

    MiniCloud::Client::ClientAccessState accessState() const noexcept;
    bool isFeatureAccessAllowed() const noexcept;

    bool connectToServer(const QString &hostname, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    MiniCloud::Client::RequestSendResult activate(const QString &licenseKey, const QString &deviceId);

    bool requestBrowse(const QString &path);
    bool requestSearch(const QString &path, const QString &query);

    bool requestCreateDirectory(const QString &parentPath, const QString &name);
    bool requestRename(const QString &path, const QString &newName);
    bool requestMove(const QString &sourcePath, const QString &destinationDirectoryPath);
    bool requestDelete(const QString &path);
    bool requestUpload(const QString &localFilePath, const QString &destinationDirectoryPath);
    bool requestDownload(const QString &remotePath, const QString &localFilePath);

signals:
    void accessStateChanged(MiniCloud::Client::ClientAccessState state);
    void activationRejected(MiniCloud::Protocol::AuthenticationStatus status);
    void activationError(const MiniCloud::Protocol::ErrorResponseData &error);
    void activationFailed(MiniCloud::Client::RequestDispatchError error);
    void connectionStateChanged(bool connected);
    void connectionFailed(const QString &message);

    void browseReceived(const QString &path, const QList<MiniCloud::Protocol::FileEntryData> &entries);
    void browseFailed(const QString &message);
    void searchReceived(const QString &path, const QList<MiniCloud::Protocol::FileEntryData> &entries);

    void fileOperationCompleted(const QString &path);
    void fileOperationFailed(const QString &message);
    void uploadProgress(quint64 bytesTransferred, quint64 totalSizeBytes);
    void downloadProgress(quint64 bytesTransferred, quint64 totalSizeBytes);

private slots:
    void onResponseReceived(MiniCloud::Client::RequestDestination destination, const MiniCloud::Protocol::ProtocolFrame &frame);

    void onErrorResponseReceived(
        MiniCloud::Client::RequestDestination destination,
        MiniCloud::Protocol::RequestId requestId,
        MiniCloud::Protocol::TaskId taskId,
        const MiniCloud::Protocol::ErrorResponseData &error);

    void onRequestFailed(
        MiniCloud::Client::RequestDestination destination,
        MiniCloud::Protocol::RequestId requestId,
        MiniCloud::Protocol::TaskId taskId,
        MiniCloud::Client::RequestDispatchError error);

    void onDisconnected();
    void onUploadCompletionFrame(const MiniCloud::Protocol::ProtocolFrame &frame);
    void onDownloadFrameReceived(const MiniCloud::Protocol::ProtocolFrame &frame);

private:
    MiniCloud::Client::ClientAccessState m_accessState{MiniCloud::Client::ClientAccessState::Locked};

    NetworkClient m_networkClient;
    RequestDispatcher m_requestDispatcher;
    MiniCloud::Protocol::RequestId m_activationRequestId{0};

    QFile m_activeUploadFile;
    MiniCloud::Protocol::RequestId m_activeUploadRequestId{0};
    MiniCloud::Protocol::TaskId m_activeUploadTaskId{0};
    quint64 m_activeUploadTotalSizeBytes{0};
    quint64 m_activeUploadBytesTransferred{0};
    bool m_uploadAwaitingReady{false};

    std::unique_ptr<QSaveFile> m_activeDownloadFile;
    MiniCloud::Protocol::RequestId m_activeDownloadRequestId{0};
    MiniCloud::Protocol::TaskId m_activeDownloadTaskId{0};
    quint64 m_activeDownloadTotalSizeBytes{0};
    quint64 m_activeDownloadBytesTransferred{0};
    bool m_downloadAwaitingStart{false};

    void sendUploadChunks();
    void resetActiveUpload();
    void resetActiveDownload();
    void setAccessState(MiniCloud::Client::ClientAccessState state);
};

Q_DECLARE_METATYPE(MiniCloud::Client::ClientAccessState)
