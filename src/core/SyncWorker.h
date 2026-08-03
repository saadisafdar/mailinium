#pragma once

#include <QObject>
#include <QTimer>
#include <QList>
#include "storage/Account.h"
#include "storage/Folder.h"
#include "storage/Message.h"

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
    
    void onFolderSelected(int messageCount);
    void onUidsFound(const QList<qint64>& uids);
    void onHeadersFetched(const QList<Message>& messages);
    void onBodyFetched(qint64 uid, const QString& body);
    
    void onTokenRefreshSuccessful(const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs);
    void onTokenRefreshFailed(const QString& errorMsg);
    
    void doSync();

private:
    Account m_account;
    ImapClient* m_imapClient = nullptr;
    GoogleOAuth* m_oauth = nullptr;
    QTimer m_syncTimer;
    
    qint64 m_currentSyncFolderId = -1;
    qint64 m_currentFetchMessageId = -1;
};
