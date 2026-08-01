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

    // Adds and starts a worker for a newly created account.
    void addAccount(const Account& account);
    
    // Returns the worker for a specific account, or nullptr if none exists.
    SyncWorker* workerForAccount(qint64 accountId) const;

signals:
    // Emitted when any worker updates its folders, so the UI can refresh.
    void foldersUpdated(qint64 accountId);
    
    // Emitted when a worker's status changes (e.g. Connecting, Error).
    void syncStatusChanged(qint64 accountId, const QString& status);

private slots:
    void onWorkerFoldersUpdated();
    void onWorkerStatusChanged(const QString& status);

private:
    SyncManager() = default;
    ~SyncManager() override;
    
    Q_DISABLE_COPY_MOVE(SyncManager)

    QMap<qint64, SyncWorker*> m_workers;
};
