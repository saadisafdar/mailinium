#include "core/SyncWorker.h"
#include "network/ImapClient.h"
#include "storage/FolderStore.h"
#include "storage/AccountStore.h"
#include "storage/MessageStore.h"
#include "protocols/MimeParser.h"
#include "auth/GoogleOAuth.h"
#include "core/Logger.h"
#include "storage/Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>

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
    connect(m_imapClient, &ImapClient::flagsFetched, this, &SyncWorker::onFlagsFetched);
    connect(m_imapClient, &ImapClient::bodyFetched, this, &SyncWorker::onBodyFetched);

    connect(m_imapClient, &ImapClient::flagsStored, this, &SyncWorker::onFlagsStored);
    connect(m_imapClient, &ImapClient::messagesCopied, this, &SyncWorker::onMessagesCopied);
    connect(m_imapClient, &ImapClient::messagesMoved, this, &SyncWorker::onMessagesMoved);
    connect(m_imapClient, &ImapClient::expunged, this, &SyncWorker::onExpunged);

    connect(&m_syncTimer, &QTimer::timeout, this, &SyncWorker::doSync);
    connect(&Settings::instance(), &Settings::settingChanged, this, &SyncWorker::onSettingChanged);
}

SyncWorker::~SyncWorker()
{
    stop();
}

void SyncWorker::start()
{
    m_authFailed = false;
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
    if (m_authFailed) {
        Logger::instance().warning(QStringLiteral("sync"), QStringLiteral("SyncWorker skipping auto-sync due to previous authentication failure."));
        return;
    }
    if (m_imapClient->state() == ImapClient::State::Disconnected) {
        Logger::instance().info(QStringLiteral("sync"), QStringLiteral("SyncWorker disconnected. Attempting to reconnect..."));
        start();
    } else if (m_imapClient->isIdle()) {
        m_imapClient->listFolders();
    }
}

void SyncWorker::syncFolder(qint64 folderId)
{
    // Prevent duplicate sync jobs for the same folder
    for (const auto& t : m_taskQueue) {
        if (t.type == ImapTask::Type::SyncFolder && t.folderId == folderId) {
            Logger::instance().debug(QStringLiteral("sync"),
                QStringLiteral("SyncFolder task for folder %1 already queued. Ignoring duplicate.").arg(folderId));
            return;
        }
    }
    if (m_currentSyncFolderId == folderId && !m_imapClient->isIdle()) {
        Logger::instance().debug(QStringLiteral("sync"),
            QStringLiteral("SyncFolder for folder %1 is currently active. Ignoring duplicate.").arg(folderId));
        return;
    }

    ImapTask task;
    task.type = ImapTask::Type::SyncFolder;
    task.folderId = folderId;
    queueTask(task);
}

void SyncWorker::fetchMessageBody(qint64 folderId, qint64 messageId)
{
    Q_UNUSED(folderId)
    ImapTask task;
    task.type = ImapTask::Type::FetchBody;
    task.messageId = messageId;
    queueTask(task);
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
    handleTaskFailure(errorMsg);
}

void SyncWorker::onLoginSuccessful()
{
    emit statusChanged(QStringLiteral("Authenticated"));
    m_authFailed = false;

    // Resume task queue processing if there are tasks
    if (!m_taskQueue.isEmpty()) {
        processNextTask();
    } else {
        // Perform initial sync
        doSync();
    }

    // Set up periodic sync using configured interval
    int intervalSecs = Settings::instance().intValue(Settings::Key::SyncIntervalSeconds);
    if (intervalSecs < 10) intervalSecs = 10; // safety minimum
    m_syncTimer.start(intervalSecs * 1000);
}

void SyncWorker::reauthenticate()
{
    Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Reauthenticating account: ") + m_account.email);
    m_authFailed = false;

    // Reload account from database in case tokens/passwords were updated
    AccountStore store;
    auto opt = store.accountById(m_account.id);
    if (opt.has_value()) {
        m_account = *opt;
    }

    if (m_imapClient->isConnected()) {
        m_imapClient->disconnectFromServer();
    }
    start();
}

void SyncWorker::onLoginFailed(const QString& reason)
{
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Login failed: ") + reason);
    emit statusChanged(QStringLiteral("Needs Attention"));
    m_authFailed = true;
    m_syncTimer.stop();
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
    store.updateOAuthTokens(m_account.id, accessToken, refreshToken, m_account.tokenExpiry);

    emit statusChanged(QStringLiteral("Authenticating (OAuth)..."));
    m_imapClient->loginOAuth2(m_account.email, m_account.accessToken);
}

void SyncWorker::onTokenRefreshFailed(const QString& errorMsg, bool isPermanent)
{
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Token refresh failed (permanent=%1): %2").arg(isPermanent).arg(errorMsg));
    if (isPermanent) {
        emit statusChanged(QStringLiteral("Needs Attention"));
        m_authFailed = true;
        m_syncTimer.stop();
    } else {
        emit statusChanged(QStringLiteral("Offline"));
    }
    m_imapClient->disconnectFromServer();
}

void SyncWorker::onSettingChanged(Settings::Key key, const QVariant& value)
{
    if (key == Settings::Key::SyncIntervalSeconds) {
        int intervalSecs = value.toInt();
        if (intervalSecs < 10) intervalSecs = 10;
        Logger::instance().info(QStringLiteral("sync"),
            QStringLiteral("Updating sync timer interval to ") + QString::number(intervalSecs) + "s for account " + QString::number(m_account.id));
        if (m_syncTimer.isActive()) {
            m_syncTimer.start(intervalSecs * 1000);
        }
    }
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

void SyncWorker::onFolderSelected(int /*messageCount*/, qint64 uidvalidity, qint64 uidnext)
{
    Q_UNUSED(uidnext)
    if (m_currentSyncFolderId == -1) return;

    FolderStore store;
    auto optFolder = store.folderById(m_currentSyncFolderId);
    if (optFolder.has_value()) {
        Folder folder = *optFolder;
        bool uidvalidityChanged = (folder.uidvalidity != 0 && folder.uidvalidity != uidvalidity);
        if (uidvalidityChanged) {
            Logger::instance().warning(QStringLiteral("sync"),
                QStringLiteral("UIDVALIDITY changed for folder %1 (local: %2, server: %3). Discarding local messages.")
                .arg(folder.name).arg(folder.uidvalidity).arg(uidvalidity));

            // Delete all local messages in the folder
            MessageStore msgStore;
            msgStore.deleteMessagesForFolder(m_currentSyncFolderId);
            emit messagesUpdated(m_currentSyncFolderId);
        }

        // Update local folder state
        folder.uidvalidity = uidvalidity;
        store.updateSyncState(folder.id, uidvalidity, folder.highestModseq);
    }

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

    // Convert to QSet for O(1) lookups and O(N) total complexity
    QSet<qint64> serverUidsSet(uids.begin(), uids.end());
    QSet<qint64> existingUidsSet(existingUids.begin(), existingUids.end());

    // Message disappearance (deleted on server)
    QList<qint64> deletedUids;
    for (qint64 uid : existingUids) {
        if (uid > 0 && !serverUidsSet.contains(uid)) {
            deletedUids.append(uid);
        }
    }

    if (!deletedUids.isEmpty()) {
        Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Detected ") + QString::number(deletedUids.size()) + " deleted messages on server. Removing from local store...");
        QSqlQuery del(::Database::connection());
        del.prepare(QStringLiteral("DELETE FROM messages WHERE folder_id = :fid AND uid = :uid"));
        for (qint64 uid : deletedUids) {
            del.bindValue(QStringLiteral(":fid"), m_currentSyncFolderId);
            del.bindValue(QStringLiteral(":uid"), uid);
            del.exec();
        }
        // Reload existing list after deletions
        existing = store.messagesForFolder(m_currentSyncFolderId, 0);
        existingUids.clear();
        for (const Message& m : existing) {
            existingUids.append(m.uid);
        }
        existingUidsSet = QSet<qint64>(existingUids.begin(), existingUids.end());
    }

    // Identify missing UIDs to download
    QList<qint64> missingUids;
    for (qint64 uid : uids) {
        if (!existingUidsSet.contains(uid)) {
            missingUids.append(uid);
        }
    }

    // Identify common UIDs to sync flags for (up to SyncFetchLimit newest messages)
    m_pendingFlagsToFetch.clear();
    QList<qint64> commonUids;
    for (qint64 uid : uids) {
        if (existingUidsSet.contains(uid)) {
            commonUids.append(uid);
        }
    }
    std::sort(commonUids.begin(), commonUids.end());
    int fetchLimit = Settings::instance().intValue(Settings::Key::SyncFetchLimit);
    if (fetchLimit <= 0) fetchLimit = 100;
    if (commonUids.size() > fetchLimit) {
        m_pendingFlagsToFetch = commonUids.mid(commonUids.size() - fetchLimit);
    } else {
        m_pendingFlagsToFetch = commonUids;
    }

    if (!missingUids.isEmpty()) {
        Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Downloading ") + QString::number(missingUids.size()) + " missing headers...");
        emit statusChanged(QStringLiteral("Downloading ") + QString::number(missingUids.size()) + " headers...");
        m_imapClient->fetchMessageHeaders(missingUids);
    } else if (!m_pendingFlagsToFetch.isEmpty()) {
        Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Syncing flags for ") + QString::number(m_pendingFlagsToFetch.size()) + " messages...");
        emit statusChanged(QStringLiteral("Syncing flags..."));
        m_imapClient->fetchMessageFlags(m_pendingFlagsToFetch);
        m_pendingFlagsToFetch.clear();
    } else {
        Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("No new messages or flags to download."));
        emit statusChanged(QStringLiteral("Synced"));
        emit messagesUpdated(m_currentSyncFolderId);

        if (!m_taskQueue.isEmpty() && m_taskQueue.head().type == ImapTask::Type::SyncFolder) {
            m_taskQueue.dequeue();
        }
        processNextTask();
    }
}

void SyncWorker::onHeadersFetched(const QList<Message>& messages)
{
    if (m_currentSyncFolderId == -1) return;

    Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Saving ") + QString::number(messages.size()) + " headers to database...");
    
    QList<Message> toInsertList;
    toInsertList.reserve(messages.size());
    for (const Message& msg : messages) {
        Message m = msg;
        m.folderId = m_currentSyncFolderId;
        toInsertList.append(m);
    }

    MessageStore store;
    QSqlDatabase db = ::Database::connection();
    bool hasTxn = db.transaction();
    store.insertMessages(toInsertList);
    if (hasTxn) {
        if (!db.commit()) {
            Logger::instance().error(QStringLiteral("sync"), QStringLiteral("COMMIT transaction failed: ") + db.lastError().text());
        }
    }

    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 7. Number inserted into SQLite: ") + QString::number(messages.size()));
    if (messages.isEmpty()) {
        Logger::instance().error(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] STOP: 0 headers inserted into SQLite!"));
    }

    emit messagesUpdated(m_currentSyncFolderId);

    if (!m_pendingFlagsToFetch.isEmpty()) {
        Logger::instance().debug(QStringLiteral("sync"), QStringLiteral("Syncing flags for ") + QString::number(m_pendingFlagsToFetch.size()) + " messages...");
        emit statusChanged(QStringLiteral("Syncing flags..."));
        m_imapClient->fetchMessageFlags(m_pendingFlagsToFetch);
        m_pendingFlagsToFetch.clear();
    } else {
        emit statusChanged(QStringLiteral("Synced"));
        if (!m_taskQueue.isEmpty() && m_taskQueue.head().type == ImapTask::Type::SyncFolder) {
            m_taskQueue.dequeue();
        }
        processNextTask();
    }
}

void SyncWorker::onFlagsFetched(const QMap<qint64, int>& flags)
{
    if (m_currentSyncFolderId == -1) return;

    Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Updating flags for ") + QString::number(flags.size()) + " messages in folder " + QString::number(m_currentSyncFolderId));
    MessageStore store;
    QSqlQuery txn(::Database::connection());
    txn.exec(QStringLiteral("BEGIN"));
    for (auto it = flags.constBegin(); it != flags.constEnd(); ++it) {
        qint64 uid = it.key();
        int serverFlags = it.value();

        auto optMsg = store.messageByUid(m_currentSyncFolderId, uid);
        if (optMsg.has_value() && optMsg->flags != serverFlags) {
            store.updateFlags(optMsg->id, serverFlags);
        }
    }
    txn.exec(QStringLiteral("COMMIT"));

    emit statusChanged(QStringLiteral("Synced"));
    emit messagesUpdated(m_currentSyncFolderId);

    if (!m_taskQueue.isEmpty() && m_taskQueue.head().type == ImapTask::Type::SyncFolder) {
        m_taskQueue.dequeue();
    }
    processNextTask();
}

void SyncWorker::onBodyFetched(qint64 uid, const QString& body)
{
    MessageStore store;
    auto optMsg = store.messageByUid(m_currentSyncFolderId, uid);
    qint64 msgId = m_currentFetchMessageId;
    if (optMsg.has_value()) {
        msgId = optMsg->id;
    }

    // Parse raw MIME payload
    MimeMessage mime = MimeParser::parse(body.toUtf8());

    if (msgId != -1) {
        store.saveMessageBodies(msgId, mime.htmlBody, mime.plainBody);
        for (const auto& att : mime.attachments) {
            store.saveAttachment(msgId, att);
        }
        for (const auto& img : mime.inlineImages) {
            store.saveAttachment(msgId, img);
        }
    }

    emit messageBodyReady(msgId, body);

    if (!m_taskQueue.isEmpty() && m_taskQueue.head().type == ImapTask::Type::FetchBody) {
        m_taskQueue.dequeue();
    }
    processNextTask();
}

static bool relocateMessage(qint64 messageId, qint64 folderId, qint64 uid)
{
    QSqlQuery q(::Database::connection());
    q.prepare(QStringLiteral("UPDATE messages SET folder_id = :fid, uid = :uid WHERE id = :id"));
    q.bindValue(QStringLiteral(":fid"), folderId);
    q.bindValue(QStringLiteral(":uid"), uid);
    q.bindValue(QStringLiteral(":id"),  messageId);
    return q.exec();
}

void SyncWorker::queueTask(const ImapTask& task)
{
    m_taskQueue.enqueue(task);
    processNextTask();
}

void SyncWorker::processNextTask()
{
    if (m_taskQueue.isEmpty()) {
        return;
    }

    if (m_imapClient->state() == ImapClient::State::Disconnected) {
        Logger::instance().info(QStringLiteral("sync"), QStringLiteral("Task queued but client is disconnected. Initiating connection..."));
        start();
        return;
    }

    if (!m_imapClient->isIdle()) {
        return; // Wait for current operation to finish
    }

    // Check for folder context drift
    const ImapTask firstTask = m_taskQueue.head();
    if (firstTask.type != ImapTask::Type::SyncFolder && firstTask.folderId != -1 && firstTask.folderId != m_currentSyncFolderId) {
        ImapTask selectTask;
        selectTask.type = ImapTask::Type::SyncFolder;
        selectTask.folderId = firstTask.folderId;
        m_taskQueue.prepend(selectTask);
    }

    const ImapTask& task = m_taskQueue.head();

    switch (task.type) {
        case ImapTask::Type::SyncFolder: {
            m_currentSyncFolderId = task.folderId;
            FolderStore store;
            auto optFolder = store.folderById(task.folderId);
            if (optFolder.has_value()) {
                emit statusChanged(QStringLiteral("Syncing ") + optFolder->name + "...");
                m_imapClient->selectFolder(optFolder->name);
            } else {
                handleTaskFailure(QStringLiteral("Folder not found"));
            }
            break;
        }
        case ImapTask::Type::FetchBody: {
            MessageStore store;
            auto optMsg = store.messageById(task.messageId);
            if (optMsg.has_value()) {
                m_currentFetchMessageId = task.messageId;
                m_imapClient->fetchMessageBody(optMsg->uid);
            } else {
                handleTaskFailure(QStringLiteral("Message not found"));
            }
            break;
        }
        case ImapTask::Type::Delete: {
            FolderStore fs;
            auto optTrash = fs.folderByRole(m_account.id, QStringLiteral("trash"));
            auto optSrc = fs.folderById(task.folderId);
            if (optSrc.has_value()) {
                if (optSrc->role == QStringLiteral("trash")) {
                    m_imapClient->storeFlags(task.uids, QStringLiteral("\\Deleted"), true);
                } else if (optTrash.has_value()) {
                    m_imapClient->moveMessages(task.uids, optTrash->name);
                } else {
                    m_imapClient->storeFlags(task.uids, QStringLiteral("\\Deleted"), true);
                }
            } else {
                handleTaskFailure(QStringLiteral("Source folder not found"));
            }
            break;
        }
        case ImapTask::Type::Archive: {
            // Archive now routes through moveMessage(); this case should never fire.
            // Log an error rather than silently deleting the message.
            Logger::instance().error(QStringLiteral("sync"), QStringLiteral("processNextTask: unexpected Archive task — archive should route through Move"));
            handleTaskFailure(QStringLiteral("Unexpected Archive task"));
            break;
        }
        case ImapTask::Type::MarkFlags: {
            m_imapClient->storeFlags(task.uids, task.flags, task.addFlags);
            break;
        }
        case ImapTask::Type::Move: {
            FolderStore fs;
            auto optDest = fs.folderById(task.destFolderId);
            if (optDest.has_value()) {
                m_imapClient->moveMessages(task.uids, optDest->name);
            } else {
                handleTaskFailure(QStringLiteral("Destination folder not found"));
            }
            break;
        }
        case ImapTask::Type::Copy: {
            FolderStore fs;
            auto optDest = fs.folderById(task.destFolderId);
            if (optDest.has_value()) {
                m_imapClient->copyMessages(task.uids, optDest->name);
            } else {
                handleTaskFailure(QStringLiteral("Destination folder not found"));
            }
            break;
        }
    }
}

void SyncWorker::handleTaskFailure(const QString& errorMsg)
{
    if (m_taskQueue.isEmpty()) return;

    if (m_imapClient->state() == ImapClient::State::Disconnected) {
        Logger::instance().warning(QStringLiteral("sync"),
            QStringLiteral("Task failed due to disconnect. Keeping task in queue for retry on reconnect. Error: ") + errorMsg);
        emit statusChanged(QStringLiteral("Offline: ") + errorMsg);
        return;
    }

    ImapTask task = m_taskQueue.dequeue();
    Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Permanent task failed: ") + errorMsg);

    MessageStore store;
    if (task.type == ImapTask::Type::Delete) {
        if (m_origFlags.contains(task.messageId)) {
            int origFlags = m_origFlags.take(task.messageId);
            store.updateFlags(task.messageId, origFlags);
            emit messagesUpdated(task.folderId);
        }
    } else if (task.type == ImapTask::Type::MarkFlags) {
        if (m_origFlags.contains(task.messageId)) {
            int origFlags = m_origFlags.take(task.messageId);
            store.updateFlags(task.messageId, origFlags);
            emit messagesUpdated(task.folderId);
        }
    } else if (task.type == ImapTask::Type::Move) {
        if (m_origFolderIds.contains(task.messageId)) {
            qint64 origFid = m_origFolderIds.take(task.messageId);
            qint64 origUid = m_origUids.take(task.messageId);
            relocateMessage(task.messageId, origFid, origUid);
            emit messagesUpdated(origFid);
            emit messagesUpdated(task.destFolderId);
        }
    } else if (task.type == ImapTask::Type::Copy) {
        QSqlQuery del(::Database::connection());
        del.prepare(QStringLiteral("DELETE FROM messages WHERE folder_id = :fid AND uid = :uid"));
        del.bindValue(QStringLiteral(":fid"), task.destFolderId);
        del.bindValue(QStringLiteral(":uid"), -1 * task.messageId);
        del.exec();
        emit messagesUpdated(task.destFolderId);
    }

    emit statusChanged(QStringLiteral("Error: ") + errorMsg);
    processNextTask();
}

void SyncWorker::deleteMessage(qint64 folderId, qint64 messageId)
{
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (!optMsg.has_value()) return;

    m_origFlags[messageId] = optMsg->flags;
    store.updateFlags(messageId, optMsg->flags | MessageFlags::Deleted);
    emit messagesUpdated(folderId);

    ImapTask task;
    task.type = ImapTask::Type::Delete;
    task.folderId = folderId;
    task.messageId = messageId;
    task.uids = { optMsg->uid };
    queueTask(task);
}

void SyncWorker::archiveMessage(qint64 folderId, qint64 messageId)
{
    FolderStore fs;
    auto optArchive = fs.folderByRole(m_account.id, QStringLiteral("archive"));
    if (optArchive.has_value()) {
        moveMessage(folderId, optArchive->id, messageId);
    } else {
        Logger::instance().error(QStringLiteral("sync"), QStringLiteral("Archive folder not found for account ") + QString::number(m_account.id));
    }
}

void SyncWorker::markMessageRead(qint64 folderId, qint64 messageId, bool read)
{
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (!optMsg.has_value()) return;

    int currentFlags = optMsg->flags;
    int newFlags = read ? (currentFlags | MessageFlags::Seen) : (currentFlags & ~MessageFlags::Seen);

    m_origFlags[messageId] = currentFlags;
    store.updateFlags(messageId, newFlags);
    emit messagesUpdated(folderId);

    ImapTask task;
    task.type = ImapTask::Type::MarkFlags;
    task.folderId = folderId;
    task.messageId = messageId;
    task.uids = { optMsg->uid };
    task.flags = QStringLiteral("\\Seen");
    task.addFlags = read;
    queueTask(task);
}

void SyncWorker::starMessage(qint64 folderId, qint64 messageId, bool starred)
{
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (!optMsg.has_value()) return;

    int currentFlags = optMsg->flags;
    int newFlags = starred ? (currentFlags | MessageFlags::Flagged) : (currentFlags & ~MessageFlags::Flagged);

    m_origFlags[messageId] = currentFlags;
    store.updateFlags(messageId, newFlags);
    emit messagesUpdated(folderId);

    ImapTask task;
    task.type = ImapTask::Type::MarkFlags;
    task.folderId = folderId;
    task.messageId = messageId;
    task.uids = { optMsg->uid };
    task.flags = QStringLiteral("\\Flagged");
    task.addFlags = starred;
    queueTask(task);
}

void SyncWorker::moveMessage(qint64 sourceFolderId, qint64 destFolderId, qint64 messageId)
{
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (!optMsg.has_value()) return;

    m_origFolderIds[messageId] = sourceFolderId;
    m_origUids[messageId] = optMsg->uid;
    qint64 tempUid = -1 * messageId;
    relocateMessage(messageId, destFolderId, tempUid);
    emit messagesUpdated(sourceFolderId);
    emit messagesUpdated(destFolderId);

    ImapTask task;
    task.type = ImapTask::Type::Move;
    task.folderId = sourceFolderId;
    task.destFolderId = destFolderId;
    task.messageId = messageId;
    task.uids = { optMsg->uid };
    queueTask(task);
}

void SyncWorker::copyMessage(qint64 sourceFolderId, qint64 destFolderId, qint64 messageId)
{
    MessageStore store;
    auto optMsg = store.messageById(messageId);
    if (!optMsg.has_value()) return;

    Message copyMsg = *optMsg;
    copyMsg.folderId = destFolderId;
    copyMsg.uid = -1 * messageId;
    store.insertMessage(copyMsg);
    emit messagesUpdated(destFolderId);

    ImapTask task;
    task.type = ImapTask::Type::Copy;
    task.folderId = sourceFolderId;
    task.destFolderId = destFolderId;
    task.messageId = messageId;
    task.uids = { optMsg->uid };
    queueTask(task);
}

void SyncWorker::refreshFolder(qint64 folderId)
{
    syncFolder(folderId);
}

void SyncWorker::onFlagsStored(const QList<qint64>& uids)
{
    Q_UNUSED(uids)
    if (m_taskQueue.isEmpty()) return;
    ImapTask task = m_taskQueue.dequeue();

    m_origFlags.remove(task.messageId);

    if (task.type == ImapTask::Type::Delete) {
        MessageStore store;
        store.deleteMessage(task.messageId);
        emit messagesUpdated(task.folderId);
    }

    emit statusChanged(QStringLiteral("Synced"));
    processNextTask();
}

void SyncWorker::onMessagesCopied(const QList<qint64>& uids)
{
    Q_UNUSED(uids)
    if (m_taskQueue.isEmpty()) return;
    ImapTask task = m_taskQueue.dequeue();

    QSqlQuery del(::Database::connection());
    del.prepare(QStringLiteral("DELETE FROM messages WHERE folder_id = :fid AND uid = :uid"));
    del.bindValue(QStringLiteral(":fid"), task.destFolderId);
    del.bindValue(QStringLiteral(":uid"), -1 * task.messageId);
    del.exec();

    emit statusChanged(QStringLiteral("Synced"));
    syncFolder(task.destFolderId);
    processNextTask();
}

void SyncWorker::onMessagesMoved(const QList<qint64>& uids)
{
    Q_UNUSED(uids)
    if (m_taskQueue.isEmpty()) return;
    ImapTask task = m_taskQueue.dequeue();

    if (task.type == ImapTask::Type::Delete) {
        MessageStore store;
        store.deleteMessage(task.messageId);
        m_origFlags.remove(task.messageId);
        emit messagesUpdated(task.folderId);
    } else if (task.type == ImapTask::Type::Move) {
        m_origFolderIds.remove(task.messageId);
        m_origUids.remove(task.messageId);

        MessageStore store;
        store.deleteMessage(task.messageId);
    }

    emit statusChanged(QStringLiteral("Synced"));
    syncFolder(task.destFolderId);
    processNextTask();
}

void SyncWorker::onExpunged()
{
    if (m_taskQueue.isEmpty()) return;
    ImapTask task = m_taskQueue.dequeue();

    emit statusChanged(QStringLiteral("Synced"));
    processNextTask();
}
