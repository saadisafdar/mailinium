#pragma once

#include <QObject>
#include <QTimer>
#include <QList>
#include "storage/Account.h"
#include "storage/Folder.h"
#include "storage/Message.h"
#include "core/Settings.h"

#include <QQueue>

struct ImapTask {
    enum class Type {
        SyncFolder,
        FetchBody,
        Delete,
        Archive,
        MarkFlags,
        Move,
        Copy
    };
    Type type;
    qint64 folderId = -1;
    qint64 messageId = -1;
    qint64 destFolderId = -1;
    QList<qint64> uids;
    QString flags;
    bool addFlags = false;
};

class ImapClient;
class GoogleOAuth;

//
// SyncWorker — Manages the synchronization lifecycle for a single account.
//
class SyncWorker : public QObject
{
    Q_OBJECT

public:
    explicit SyncWorker(const Account& account, QObject* parent = nullptr);
    ~SyncWorker() override;

    void start();
    void stop();

    qint64 accountId() const { return m_account.id; }

    void syncFolder(qint64 folderId);
    void fetchMessageBody(qint64 folderId, qint64 messageId);

    // Interactive message operations
    void deleteMessage(qint64 folderId, qint64 messageId);
    void archiveMessage(qint64 folderId, qint64 messageId);
    void markMessageRead(qint64 folderId, qint64 messageId, bool read);
    void starMessage(qint64 folderId, qint64 messageId, bool starred);
    void moveMessage(qint64 sourceFolderId, qint64 destFolderId, qint64 messageId);
    void copyMessage(qint64 sourceFolderId, qint64 destFolderId, qint64 messageId);
    void refreshFolder(qint64 folderId);

signals:
    void statusChanged(const QString& status);
    void foldersUpdated();
    void messagesUpdated(qint64 folderId);
    void messageBodyReady(qint64 messageId, const QString& body);

private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onClientError(const QString& errorMsg);
    void onLoginSuccessful();
    void onLoginFailed(const QString& reason);
    void onFoldersListed(const QList<Folder>& folders);
    void onListFailed(const QString& reason);

    void onFolderSelected(int messageCount, qint64 uidvalidity, qint64 uidnext);
    void onUidsFound(const QList<qint64>& uids);
    void onHeadersFetched(const QList<Message>& messages);
    void onFlagsFetched(const QMap<qint64, int>& flags);
    void onBodyFetched(qint64 uid, const QString& body);

    void onFlagsStored(const QList<qint64>& uids);
    void onMessagesCopied(const QList<qint64>& uids);
    void onMessagesMoved(const QList<qint64>& uids);
    void onExpunged();

    void onTokenRefreshSuccessful(const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs);
    void onTokenRefreshFailed(const QString& errorMsg);

    void onSettingChanged(Settings::Key key, const QVariant& value);

    void doSync();

private:
    void queueTask(const ImapTask& task);
    void processNextTask();
    void handleTaskFailure(const QString& errorMsg);

    Account m_account;
    ImapClient* m_imapClient = nullptr;
    GoogleOAuth* m_oauth = nullptr;
    QTimer m_syncTimer;

    qint64 m_currentSyncFolderId = -1;
    qint64 m_currentFetchMessageId = -1;

    QQueue<ImapTask> m_taskQueue;
    QMap<qint64, qint64> m_origFolderIds;
    QMap<qint64, qint64> m_origUids;
    QMap<qint64, int> m_origFlags;
    QList<qint64> m_pendingFlagsToFetch;
    bool m_authFailed = false;
};
