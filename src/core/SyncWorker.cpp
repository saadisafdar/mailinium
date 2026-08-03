#include "core/SyncWorker.h"
#include "network/ImapClient.h"
#include "storage/FolderStore.h"
#include "storage/AccountStore.h"
#include "storage/MessageStore.h"
#include "auth/GoogleOAuth.h"
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

    connect(m_imapClient, &ImapClient::folderSelected, this, &SyncWorker::onFolderSelected);
    connect(m_imapClient, &ImapClient::uidsFound, this, &SyncWorker::onUidsFound);
    connect(m_imapClient, &ImapClient::headersFetched, this, &SyncWorker::onHeadersFetched);
    connect(m_imapClient, &ImapClient::bodyFetched, this, &SyncWorker::onBodyFetched);

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
    // List folders initially
    m_imapClient->listFolders();
}

void SyncWorker::syncFolder(qint64 folderId)
{
    m_currentSyncFolderId = folderId;
    
    FolderStore store;
    auto optFolder = store.folderById(folderId);
    if (optFolder.has_value()) {
        emit statusChanged(QStringLiteral("Syncing ") + optFolder->name + "...");
        m_imapClient->selectFolder(optFolder->name);
    }
}

void SyncWorker::fetchMessageBody(qint64 folderId, qint64 messageId)
{
    Q_UNUSED(folderId)
    
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (optMsg.has_value()) {
        if (optMsg->bodyFetched) {
            // Already fetched, UI will load it from DB (in Phase 3E) or we can emit it
            // For now just fetch it again to fulfill the requirement of loading it dynamically
        }
        
        m_currentFetchMessageId = messageId;
        m_imapClient->fetchMessageBody(optMsg->uid);
    }
}

void SyncWorker::onClientConnected()
{
    if (m_account.authType == AuthType::OAuth2) {
        // Check if token is expired (e.g. within 60 seconds of expiring)
        if (m_account.tokenExpiry.isValid() && m_account.tokenExpiry <= QDateTime::currentDateTimeUtc().addSecs(60)) {
            emit statusChanged(QStringLiteral("Refreshing token..."));
            if (!m_oauth) {
                m_oauth = new GoogleOAuth(this);
                connect(m_oauth, &GoogleOAuth::authSuccessful, this, &SyncWorker::onTokenRefreshSuccessful);
                connect(m_oauth, &GoogleOAuth::authFailed, this, &SyncWorker::onTokenRefreshFailed);
            }
            m_oauth->refreshAccessToken(m_account.refreshToken);
        } else {
            emit statusChanged(QStringLiteral("Authenticating (OAuth)..."));
            m_imapClient->loginOAuth2(m_account.email, m_account.accessToken);
        }
    } else {
        // Enforce OAuth for Google accounts; do not allow App Passwords
        if (m_account.provider.toLower() == QStringLiteral("google") || 
            m_account.imapHost.toLower().contains(QStringLiteral("gmail.com"))) {
            Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Password authentication is not supported for Google accounts. Please use OAuth 2.0."));
            emit statusChanged(QStringLiteral("OAuth 2.0 Required"));
            m_imapClient->disconnectFromServer();
            return;
        }

        emit statusChanged(QStringLiteral("Authenticating..."));
        m_imapClient->login(m_account.email, m_account.password);
    }
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

void SyncWorker::onTokenRefreshSuccessful(const QString& /*email*/, const QString& accessToken, const QString& refreshToken, int expiresInSecs)
{
    Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Token refreshed for ") + m_account.email);
    
    // Update local account model
    m_account.accessToken = accessToken;
    m_account.refreshToken = refreshToken;
    m_account.tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresInSecs);
    
    // Update database
    AccountStore store;
    store.updateAccount(m_account);
    
    emit statusChanged(QStringLiteral("Authenticating (OAuth)..."));
    m_imapClient->loginOAuth2(m_account.email, m_account.accessToken);
}

void SyncWorker::onTokenRefreshFailed(const QString& errorMsg)
{
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Token refresh failed: ") + errorMsg);
    emit statusChanged(QStringLiteral("Token refresh failed"));
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

void SyncWorker::onFolderSelected(int /*messageCount*/)
{
    m_imapClient->searchAllUids();
}

void SyncWorker::onUidsFound(const QList<qint64>& uids)
{
    if (m_currentSyncFolderId == -1) return;
    
    MessageStore store;
    QList<Message> existing = store.messagesForFolder(m_currentSyncFolderId, 0); // get all
    QList<qint64> existingUids;
    for (const Message& m : existing) {
        existingUids.append(m.uid);
    }
    
    Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Found ") + QString::number(uids.size()) + " UIDs on server. " + QString::number(existing.size()) + " exist locally.");
    
    QList<qint64> missingUids;
    for (qint64 uid : uids) {
        if (!existingUids.contains(uid)) {
            missingUids.append(uid);
        }
    }
    
    if (missingUids.isEmpty()) {
        Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("No new messages to download."));
        emit statusChanged(QStringLiteral("Synced"));
        emit messagesUpdated(m_currentSyncFolderId);
    } else {
        Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Downloading ") + QString::number(missingUids.size()) + " missing headers...");
        emit statusChanged(QStringLiteral("Downloading ") + QString::number(missingUids.size()) + " headers...");
        m_imapClient->fetchMessageHeaders(missingUids);
    }
}

void SyncWorker::onHeadersFetched(const QList<Message>& messages)
{
    if (m_currentSyncFolderId == -1) return;
    
    Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Saving ") + QString::number(messages.size()) + " headers to database...");
    MessageStore store;
    for (const Message& msg : messages) {
        Message toInsert = msg;
        toInsert.folderId = m_currentSyncFolderId;
        store.insertMessage(toInsert);
    }
    
    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 7. Number inserted into SQLite: ") + QString::number(messages.size()));
    if (messages.isEmpty()) {
        Logger::instance().error(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] STOP: 0 headers inserted into SQLite!"));
    }
    
    emit statusChanged(QStringLiteral("Synced"));
    emit messagesUpdated(m_currentSyncFolderId);
}

void SyncWorker::onBodyFetched(qint64 uid, const QString& body)
{
    // Phase 3C: we just emit it for the view. We'll persist in Phase 3E.
    MessageStore store;
    auto optMsg = store.messageByUid(m_currentSyncFolderId, uid);
    qint64 msgId = m_currentFetchMessageId;
    if (optMsg.has_value()) {
        msgId = optMsg->id;
        store.markBodyFetched(msgId);
    }
    
    emit messageBodyReady(msgId, body);
}
