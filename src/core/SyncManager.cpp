#include "core/SyncManager.h"
#include "core/SyncWorker.h"
#include "storage/AccountStore.h"
#include "core/Logger.h"

SyncManager& SyncManager::instance()
{
    static SyncManager s_instance;
    return s_instance;
}

SyncManager::~SyncManager()
{
    stopAll();
}

void SyncManager::startAll()
{
    AccountStore store;
    const auto accounts = store.allAccounts();
    
    Logger::instance().info(QStringLiteral("sync_manager"), QStringLiteral("Starting SyncManager. Accounts found: ") + QString::number(accounts.size()));
    
    for (const Account& acc : accounts) {
        addAccount(acc);
    }
}

void SyncManager::stopAll()
{
    for (SyncWorker* worker : std::as_const(m_workers)) {
        worker->stop();
        worker->deleteLater();
    }
    m_workers.clear();
}

void SyncManager::addAccount(const Account& account)
{
    if (m_workers.contains(account.id)) {
        return; // Already tracking this account
    }

    SyncWorker* worker = new SyncWorker(account, this);
    
    connect(worker, &SyncWorker::foldersUpdated, this, &SyncManager::onWorkerFoldersUpdated);
    connect(worker, &SyncWorker::statusChanged, this, &SyncManager::onWorkerStatusChanged);
    
    m_workers.insert(account.id, worker);
    Logger::instance().info(QStringLiteral("sync_manager"), QStringLiteral("Added account ") + QString::number(account.id) + ". Starting worker...");
    worker->start();
}

SyncWorker* SyncManager::workerForAccount(qint64 accountId) const
{
    return m_workers.value(accountId, nullptr);
}

void SyncManager::onWorkerFoldersUpdated()
{
    auto* worker = qobject_cast<SyncWorker*>(sender());
    if (worker) {
        Logger::instance().info(QStringLiteral("sync_manager"), QStringLiteral("Folders updated for account: ") + QString::number(worker->accountId()));
        emit foldersUpdated(worker->accountId());
    }
}

void SyncManager::onWorkerStatusChanged(const QString& status)
{
    auto* worker = qobject_cast<SyncWorker*>(sender());
    if (worker) {
        emit syncStatusChanged(worker->accountId(), status);
    }
}
