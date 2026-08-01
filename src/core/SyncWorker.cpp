#include "core/SyncWorker.h"
#include "network/ImapClient.h"
#include "storage/FolderStore.h"
#include "core/Logger.h"

SyncWorker::SyncWorker(const Account& account, QObject* parent)
    : QObject(parent), m_account(account)
{
    m_imapClient = new ImapClient(this);

    connect(m_imapClient, &ImapClient::connected, this, &SyncWorker::onClientConnected);
    connect(m_imapClient, &ImapClient::disconnected, this, &SyncWorker::onClientDisconnected);
    connect(m_imapClient, &ImapClient::errorOccurred, this, &SyncWorker::onClientError);
    connect(m_imapClient, &ImapClient::loginSuccessful, this, &SyncWorker::onLoginSuccessful);
    connect(m_imapClient, &ImapClient::loginFailed, this, &SyncWorker::onLoginFailed);
    connect(m_imapClient, &ImapClient::foldersListed, this, &SyncWorker::onFoldersListed);
    connect(m_imapClient, &ImapClient::listFailed, this, &SyncWorker::onListFailed);

    connect(&m_syncTimer, &QTimer::timeout, this, &SyncWorker::doSync);
}

SyncWorker::~SyncWorker()
{
    stop();
}

void SyncWorker::start()
{
    Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Starting SyncWorker for ") + m_account.email);
    emit statusChanged(QStringLiteral("Connecting..."));
    m_imapClient->connectToServer(m_account.imapHost, m_account.imapPort);
}

void SyncWorker::stop()
{
    Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Stopping SyncWorker for ") + m_account.email);
    m_syncTimer.stop();
    m_imapClient->disconnectFromServer();
    emit statusChanged(QStringLiteral("Disconnected"));
}

void SyncWorker::doSync()
{
    // In a real client, this might check for new messages.
    // For Phase 3C, we just list folders.
    m_imapClient->listFolders();
}

void SyncWorker::onClientConnected()
{
    emit statusChanged(QStringLiteral("Authenticating..."));
    m_imapClient->login(m_account.email, m_account.password);
}

void SyncWorker::onClientDisconnected()
{
    emit statusChanged(QStringLiteral("Disconnected"));
}

void SyncWorker::onClientError(const QString& errorMsg)
{
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("IMAP error: ") + errorMsg);
    emit statusChanged(QStringLiteral("Error: ") + errorMsg);
}

void SyncWorker::onLoginSuccessful()
{
    emit statusChanged(QStringLiteral("Authenticated"));
    
    // Perform initial sync
    doSync();
    
    // Set up periodic sync (e.g., every 5 minutes)
    m_syncTimer.start(5 * 60 * 1000);
}

void SyncWorker::onLoginFailed(const QString& reason)
{
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Login failed: ") + reason);
    emit statusChanged(QStringLiteral("Login failed"));
    m_imapClient->disconnectFromServer();
}

void SyncWorker::onFoldersListed(const QList<Folder>& folders)
{
    emit statusChanged(QStringLiteral("Folders synced"));
    
    FolderStore store;
    bool changesMade = false;
    
    for (const Folder& f : folders) {
        // Check if folder exists
        auto existingFolder = store.folderByName(m_account.id, f.name);
        if (!existingFolder.has_value()) {
            Folder newFolder = f;
            newFolder.accountId = m_account.id;
            store.insertFolder(newFolder);
            changesMade = true;
            Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Added folder: ") + newFolder.name);
        }
    }
    
    if (changesMade) {
        emit foldersUpdated();
    }
}

void SyncWorker::onListFailed(const QString& reason)
{
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Failed to list folders: ") + reason);
    emit statusChanged(QStringLiteral("Sync failed"));
}
