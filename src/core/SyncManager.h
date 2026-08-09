#pragma once

#include <QObject>
#include <QMap>

#include "storage/Account.h"

class SyncWorker;

//
// SyncManager — Manages SyncWorkers for all configured accounts.
//
class SyncManager : public QObject
{
    Q_OBJECT

public:
    static SyncManager& instance();

    // Starts workers for all accounts currently in the database.
    void startAll();

    // Stops all workers.
    void stopAll();
    
    // Stops and removes the worker for a specific account.
    void stopWorker(qint64 accountId);

    // Adds and starts a worker for a newly created account.
    void addAccount(const Account& account);
    
    // Returns the worker for a specific account, or nullptr if none exists.
    SyncWorker* workerForAccount(qint64 accountId) const;

    void syncFolder(qint64 accountId, qint64 folderId);
    void fetchMessageBody(qint64 accountId, qint64 folderId, qint64 messageId);

    void deleteMessage(qint64 accountId, qint64 folderId, qint64 messageId);
    void archiveMessage(qint64 accountId, qint64 folderId, qint64 messageId);
    void markMessageRead(qint64 accountId, qint64 folderId, qint64 messageId, bool read);
    void starMessage(qint64 accountId, qint64 folderId, qint64 messageId, bool starred);
    void moveMessage(qint64 accountId, qint64 sourceFolderId, qint64 destFolderId, qint64 messageId);
    void copyMessage(qint64 accountId, qint64 sourceFolderId, qint64 destFolderId, qint64 messageId);
    void refreshFolder(qint64 accountId, qint64 folderId);

signals:
    // Emitted when any worker updates its folders, so the UI can refresh.
    void foldersUpdated(qint64 accountId);
    
    // Emitted when a worker's status changes (e.g. Connecting, Error).
    void syncStatusChanged(qint64 accountId, const QString& status);

    void messagesUpdated(qint64 accountId, qint64 folderId);
    void messageBodyReady(qint64 accountId, qint64 messageId, const QString& body);

private slots:
    void onWorkerFoldersUpdated();
    void onWorkerStatusChanged(const QString& status);
    void onWorkerMessagesUpdated(qint64 folderId);
    void onWorkerMessageBodyReady(qint64 messageId, const QString& body);

private:
    SyncManager() = default;
    ~SyncManager() override;
    
    Q_DISABLE_COPY_MOVE(SyncManager)

    QMap<qint64, SyncWorker*> m_workers;
};
