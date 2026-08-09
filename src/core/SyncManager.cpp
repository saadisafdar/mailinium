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

void SyncManager::stopWorker(qint64 accountId)
{
    if (m_workers.contains(accountId)) {
        SyncWorker* worker = m_workers.take(accountId);
        worker->stop();
        worker->deleteLater();
        Logger::instance().info(QStringLiteral("sync_manager"), QStringLiteral("Stopped worker for account ") + QString::number(accountId));
    }
}

void SyncManager::addAccount(const Account& account)
{
    if (m_workers.contains(account.id)) {
        return; // Already tracking this account
    }

    SyncWorker* worker = new SyncWorker(account, this);
    
    connect(worker, &SyncWorker::foldersUpdated, this, &SyncManager::onWorkerFoldersUpdated);
    connect(worker, &SyncWorker::statusChanged, this, &SyncManager::onWorkerStatusChanged);
    connect(worker, &SyncWorker::messagesUpdated, this, &SyncManager::onWorkerMessagesUpdated);
    connect(worker, &SyncWorker::messageBodyReady, this, &SyncManager::onWorkerMessageBodyReady);
    
    m_workers.insert(account.id, worker);
    Logger::instance().info(QStringLiteral("sync_manager"), QStringLiteral("Added account ") + QString::number(account.id) + ". Starting worker...");
    worker->start();
}

SyncWorker* SyncManager::workerForAccount(qint64 accountId) const
{
    return m_workers.value(accountId, nullptr);
}

void SyncManager::syncFolder(qint64 accountId, qint64 folderId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->syncFolder(folderId);
    }
}

void SyncManager::fetchMessageBody(qint64 accountId, qint64 folderId, qint64 messageId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->fetchMessageBody(folderId, messageId);
    }
}

void SyncManager::deleteMessage(qint64 accountId, qint64 folderId, qint64 messageId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->deleteMessage(folderId, messageId);
    }
}

void SyncManager::archiveMessage(qint64 accountId, qint64 folderId, qint64 messageId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->archiveMessage(folderId, messageId);
    }
}

void SyncManager::markMessageRead(qint64 accountId, qint64 folderId, qint64 messageId, bool read)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->markMessageRead(folderId, messageId, read);
    }
}

void SyncManager::starMessage(qint64 accountId, qint64 folderId, qint64 messageId, bool starred)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->starMessage(folderId, messageId, starred);
    }
}

void SyncManager::moveMessage(qint64 accountId, qint64 sourceFolderId, qint64 destFolderId, qint64 messageId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->moveMessage(sourceFolderId, destFolderId, messageId);
    }
}

void SyncManager::copyMessage(qint64 accountId, qint64 sourceFolderId, qint64 destFolderId, qint64 messageId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->copyMessage(sourceFolderId, destFolderId, messageId);
    }
}

void SyncManager::refreshFolder(qint64 accountId, qint64 folderId)
{
    auto* worker = workerForAccount(accountId);
    if (worker) {
        worker->refreshFolder(folderId);
    }
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

void SyncManager::onWorkerMessagesUpdated(qint64 folderId)
{
    auto* worker = qobject_cast<SyncWorker*>(sender());
    if (worker) {
        emit messagesUpdated(worker->accountId(), folderId);
    }
}

void SyncManager::onWorkerMessageBodyReady(qint64 messageId, const QString& body)
{
    auto* worker = qobject_cast<SyncWorker*>(sender());
    if (worker) {
        emit messageBodyReady(worker->accountId(), messageId, body);
    }
}
