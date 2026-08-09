#include "MessageStore.h"
#include "Database.h"
#include "../core/Logger.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>

// ---------------------------------------------------------------------------
// Helper: map one QSqlQuery row to a Message
// ---------------------------------------------------------------------------

static Message messageFromQuery(const QSqlQuery& q)
{
    Message m;
    m.id          = q.value(0).toLongLong();
    m.folderId    = q.value(1).toLongLong();
    m.uid         = q.value(2).toLongLong();
    m.messageId   = q.value(3).toString();
    m.threadId    = q.value(4).toLongLong();
    m.subject     = q.value(5).toString();
    m.fromAddress = q.value(6).toString();
    m.fromName    = q.value(7).toString();

    // date_utc stored as ISO 8601 string ("2025-01-15T14:23:01Z")
    m.date = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
    m.date.setTimeZone(QTimeZone::UTC);

    m.flags       = q.value(9).toInt();
    m.size        = q.value(10).toLongLong();
    m.bodyFetched = q.value(11).toInt() != 0;
    m.toList      = q.value(12).toString();
    m.ccList      = q.value(13).toString();
    m.bccList     = q.value(14).toString();
    m.replyTo     = q.value(15).toString();
    return m;
}

// Column order used throughout — update messageFromQuery() if this changes.
static constexpr auto kSelectColumns =
    "SELECT id, folder_id, uid, message_id, thread_id,"
    "       subject, from_address, from_name, date_utc,"
    "       flags, size, body_fetched, to_list, cc_list, bcc_list, reply_to"
    "  FROM messages";

// ---------------------------------------------------------------------------
// Insert
// ---------------------------------------------------------------------------

qint64 MessageStore::insertMessage(const Message& msg)
{
    // INSERT OR IGNORE: if this (folder_id, uid) already exists, do nothing.
    // We then SELECT back the id regardless so the caller always gets a valid id.
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO messages"
        "    (folder_id, uid, message_id, thread_id,"
        "     subject, from_address, from_name, date_utc,"
        "     flags, size, body_fetched, to_list, cc_list, bcc_list, reply_to)"
        "    VALUES(:folderId, :uid, :messageId, :threadId,"
        "           :subject, :fromAddress, :fromName, :dateUtc,"
        "           :flags, :size, :bodyFetched, :toList, :ccList, :bccList, :replyTo)"
    ));

    q.bindValue(QStringLiteral(":folderId"),    msg.folderId);
    q.bindValue(QStringLiteral(":uid"),         msg.uid);
    q.bindValue(QStringLiteral(":messageId"),   msg.messageId.isNull() ? QStringLiteral("") : msg.messageId);
    q.bindValue(QStringLiteral(":threadId"),    msg.threadId);
    q.bindValue(QStringLiteral(":subject"),     msg.subject.isNull() ? QStringLiteral("") : msg.subject);
    q.bindValue(QStringLiteral(":fromAddress"), msg.fromAddress.isNull() ? QStringLiteral("") : msg.fromAddress);
    q.bindValue(QStringLiteral(":fromName"),    msg.fromName.isNull() ? QStringLiteral("") : msg.fromName);
    q.bindValue(QStringLiteral(":dateUtc"),
        msg.date.isValid()
            ? msg.date.toTimeZone(QTimeZone::UTC).toString(Qt::ISODate)
            : QString{});
    q.bindValue(QStringLiteral(":flags"),       msg.flags);
    q.bindValue(QStringLiteral(":size"),        msg.size);
    q.bindValue(QStringLiteral(":bodyFetched"), msg.bodyFetched ? 1 : 0);
    q.bindValue(QStringLiteral(":toList"),      msg.toList.isNull() ? QStringLiteral("") : msg.toList);
    q.bindValue(QStringLiteral(":ccList"),      msg.ccList.isNull() ? QStringLiteral("") : msg.ccList);
    q.bindValue(QStringLiteral(":bccList"),     msg.bccList.isNull() ? QStringLiteral("") : msg.bccList);
    q.bindValue(QStringLiteral(":replyTo"),     msg.replyTo.isNull() ? QStringLiteral("") : msg.replyTo);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("insertMessage failed: ") + q.lastError().text());
        return -1;
    }

    // lastInsertId() returns 0 for OR IGNORE no-ops in SQLite.
    const QVariant lastId = q.lastInsertId();
    if (!lastId.isNull() && lastId.toLongLong() > 0) {
        return lastId.toLongLong();
    }

    // Row already existed — retrieve its id
    QSqlQuery sel(Database::connection());
    sel.prepare(QStringLiteral(
        "SELECT id FROM messages WHERE folder_id = :fid AND uid = :uid"));
    sel.bindValue(QStringLiteral(":fid"), msg.folderId);
    sel.bindValue(QStringLiteral(":uid"), msg.uid);

    if (!sel.exec() || !sel.next()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("insertMessage: cannot retrieve id for existing uid ") +
            QString::number(msg.uid));
        return -1;
    }

    return sel.value(0).toLongLong();
}

// ---------------------------------------------------------------------------
// Updates
// ---------------------------------------------------------------------------

bool MessageStore::updateFlags(qint64 messageId, int flags)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "UPDATE messages SET flags = :flags WHERE id = :id"));
    q.bindValue(QStringLiteral(":flags"), flags);
    q.bindValue(QStringLiteral(":id"),    messageId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("updateFlags failed for message ")
            + QString::number(messageId) + QStringLiteral(": ")
            + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

bool MessageStore::markBodyFetched(qint64 messageId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "UPDATE messages SET body_fetched = 1 WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), messageId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("markBodyFetched failed for message ")
            + QString::number(messageId) + QStringLiteral(": ")
            + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

// ---------------------------------------------------------------------------
// Deletes
// ---------------------------------------------------------------------------

bool MessageStore::deleteMessage(qint64 messageId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral("DELETE FROM messages WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), messageId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("deleteMessage failed for id ")
            + QString::number(messageId) + QStringLiteral(": ")
            + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

bool MessageStore::deleteMessagesForFolder(qint64 folderId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "DELETE FROM messages WHERE folder_id = :fid"));
    q.bindValue(QStringLiteral(":fid"), folderId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("deleteMessagesForFolder failed for folder ")
            + QString::number(folderId) + QStringLiteral(": ")
            + q.lastError().text());
        return false;
    }

    return true; // 0 rows affected is still a success
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::optional<Message> MessageStore::messageById(qint64 id)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral("%1 WHERE id = :id")
        .arg(QString::fromLatin1(kSelectColumns)));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("messageById failed: ") + q.lastError().text());
        return std::nullopt;
    }

    return q.next() ? std::optional<Message>(messageFromQuery(q)) : std::nullopt;
}

std::optional<Message> MessageStore::messageByUid(qint64 folderId, qint64 uid)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "%1 WHERE folder_id = :fid AND uid = :uid")
        .arg(QString::fromLatin1(kSelectColumns)));
    q.bindValue(QStringLiteral(":fid"), folderId);
    q.bindValue(QStringLiteral(":uid"), uid);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("messageByUid failed: ") + q.lastError().text());
        return std::nullopt;
    }

    return q.next() ? std::optional<Message>(messageFromQuery(q)) : std::nullopt;
}

QList<Message> MessageStore::messagesForFolder(qint64 folderId,
                                                int limit,
                                                int offset)
{
    bool isTrash = false;
    QSqlQuery fq(Database::connection());
    fq.prepare(QStringLiteral("SELECT role FROM folders WHERE id = :fid"));
    fq.bindValue(QStringLiteral(":fid"), folderId);
    if (fq.exec() && fq.next()) {
        isTrash = (fq.value(0).toString() == QStringLiteral("trash"));
    }

    QString sql = QStringLiteral("%1 WHERE folder_id = :fid AND uid >= 0")
                  .arg(QString::fromLatin1(kSelectColumns));
                  
    if (!isTrash) {
        sql += QStringLiteral(" AND (flags & :deletedFlag) = 0");
    }
    
    sql += QStringLiteral(" ORDER BY date_utc DESC");

    if (limit > 0) {
        sql += QStringLiteral(" LIMIT %1 OFFSET %2")
               .arg(limit).arg(offset);
    }

    QSqlQuery q(Database::connection());
    q.prepare(sql);
    q.bindValue(QStringLiteral(":fid"), folderId);
    if (!isTrash) {
        q.bindValue(QStringLiteral(":deletedFlag"), MessageFlags::Deleted);
    }

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("messagesForFolder failed: ") + q.lastError().text());
        return {};
    }

    QList<Message> result;
    while (q.next()) {
        result.append(messageFromQuery(q));
    }

    return result;
}

int MessageStore::messageCount(qint64 folderId)
{
    bool isTrash = false;
    QSqlQuery fq(Database::connection());
    fq.prepare(QStringLiteral("SELECT role FROM folders WHERE id = :fid"));
    fq.bindValue(QStringLiteral(":fid"), folderId);
    if (fq.exec() && fq.next()) {
        isTrash = (fq.value(0).toString() == QStringLiteral("trash"));
    }

    QString sql = QStringLiteral("SELECT COUNT(*) FROM messages WHERE folder_id = :fid");
    if (!isTrash) {
        sql += QStringLiteral(" AND (flags & :deletedFlag) = 0");
    }

    QSqlQuery q(Database::connection());
    q.prepare(sql);
    q.bindValue(QStringLiteral(":fid"), folderId);
    if (!isTrash) {
        q.bindValue(QStringLiteral(":deletedFlag"), MessageFlags::Deleted);
    }

    if (!q.exec() || !q.next()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("messageCount failed: ") + q.lastError().text());
        return 0;
    }

    return q.value(0).toInt();
}

int MessageStore::unreadCount(qint64 folderId)
{
    bool isTrash = false;
    QSqlQuery fq(Database::connection());
    fq.prepare(QStringLiteral("SELECT role FROM folders WHERE id = :fid"));
    fq.bindValue(QStringLiteral(":fid"), folderId);
    if (fq.exec() && fq.next()) {
        isTrash = (fq.value(0).toString() == QStringLiteral("trash"));
    }

    QString sql = QStringLiteral("SELECT COUNT(*) FROM messages WHERE folder_id = :fid AND (flags & :seenFlag) = 0");
    if (!isTrash) {
        sql += QStringLiteral(" AND (flags & :deletedFlag) = 0");
    }

    QSqlQuery q(Database::connection());
    q.prepare(sql);
    q.bindValue(QStringLiteral(":fid"),      folderId);
    q.bindValue(QStringLiteral(":seenFlag"), MessageFlags::Seen);
    if (!isTrash) {
        q.bindValue(QStringLiteral(":deletedFlag"), MessageFlags::Deleted);
    }

    if (!q.exec() || !q.next()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("unreadCount failed: ") + q.lastError().text());
        return 0;
    }

    return q.value(0).toInt();
}

// ---------------------------------------------------------------------------
// Body & Attachments (Phase 4A)
// ---------------------------------------------------------------------------

bool MessageStore::saveMessageBodies(qint64 messageId, const QString& htmlBody, const QString& plainBody)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO message_bodies(message_id, html_body, plain_body)"
        "    VALUES(:mid, :html, :plain)"
        "    ON CONFLICT(message_id) DO UPDATE SET"
        "        html_body = excluded.html_body,"
        "        plain_body = excluded.plain_body"
    ));
    q.bindValue(QStringLiteral(":mid"),   messageId);
    q.bindValue(QStringLiteral(":html"),  htmlBody.isNull() ? QStringLiteral("") : htmlBody);
    q.bindValue(QStringLiteral(":plain"), plainBody.isNull() ? QStringLiteral("") : plainBody);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("saveMessageBodies failed for message ") + QString::number(messageId) + ": " + q.lastError().text());
        return false;
    }

    markBodyFetched(messageId);
    return true;
}

std::pair<QString, QString> MessageStore::getMessageBodies(qint64 messageId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "SELECT html_body, plain_body FROM message_bodies WHERE message_id = :mid"
    ));
    q.bindValue(QStringLiteral(":mid"), messageId);

    if (!q.exec() || !q.next()) {
        return { QString(), QString() };
    }

    return { q.value(0).toString(), q.value(1).toString() };
}

bool MessageStore::saveAttachment(qint64 messageId, const AttachmentInfo& attachment)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO attachments(message_id, filename, content_type, content_id, size, is_inline, data)"
        "    VALUES(:mid, :filename, :ctype, :cid, :size, :inline, :data)"
    ));
    q.bindValue(QStringLiteral(":mid"),      messageId);
    q.bindValue(QStringLiteral(":filename"), attachment.filename.isNull() ? QStringLiteral("") : attachment.filename);
    q.bindValue(QStringLiteral(":ctype"),    attachment.contentType.isNull() ? QStringLiteral("") : attachment.contentType);
    q.bindValue(QStringLiteral(":cid"),      attachment.contentId.isNull() ? QStringLiteral("") : attachment.contentId);
    q.bindValue(QStringLiteral(":size"),     attachment.size);
    q.bindValue(QStringLiteral(":inline"),   attachment.isInline ? 1 : 0);
    q.bindValue(QStringLiteral(":data"),     attachment.data);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("saveAttachment failed for message ") + QString::number(messageId) + ": " + q.lastError().text());
        return false;
    }

    return true;
}

QList<AttachmentInfo> MessageStore::getAttachments(qint64 messageId, bool loadInlineDataOnly)
{
    QSqlQuery q(Database::connection());
    if (loadInlineDataOnly) {
        q.prepare(QStringLiteral(
            "SELECT id, message_id, filename, content_type, content_id, size, is_inline, "
            "       CASE WHEN is_inline = 1 OR (content_id IS NOT NULL AND content_id != '') THEN data ELSE NULL END "
            "  FROM attachments WHERE message_id = :mid"
        ));
    } else {
        q.prepare(QStringLiteral(
            "SELECT id, message_id, filename, content_type, content_id, size, is_inline, data "
            "  FROM attachments WHERE message_id = :mid"
        ));
    }
    q.bindValue(QStringLiteral(":mid"), messageId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("getAttachments failed for message ") + QString::number(messageId) + ": " + q.lastError().text());
        return {};
    }

    QList<AttachmentInfo> list;
    while (q.next()) {
        AttachmentInfo att;
        att.id          = q.value(0).toLongLong();
        att.messageId   = q.value(1).toLongLong();
        att.filename    = q.value(2).toString();
        att.contentType = q.value(3).toString();
        att.contentId   = q.value(4).toString();
        att.size        = q.value(5).toLongLong();
        att.isInline    = q.value(6).toInt() != 0;
        att.data        = q.value(7).toByteArray();
        list.append(att);
    }

    return list;
}

QByteArray MessageStore::getAttachmentData(qint64 attachmentId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "SELECT data FROM attachments WHERE id = :id"
    ));
    q.bindValue(QStringLiteral(":id"), attachmentId);

    if (!q.exec() || !q.next()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("getAttachmentData failed for id ") + QString::number(attachmentId));
        return {};
    }

    return q.value(0).toByteArray();
}

QList<Message> MessageStore::searchMessages(const QString& query, qint64 accountId, qint64 folderId)
{
    if (query.trimmed().isEmpty()) {
        return {};
    }

    // Sanitize query for FTS5: escape internal double-quotes, then wrap in double-quotes.
    // The trailing * (prefix match) must be OUTSIDE the quoted phrase per FTS5 grammar.
    // E.g. user types "Hell" → FTS5 receives "Hell"* → matches "Hello", "Hellfire", etc.
    // Special characters like '(', ')', ':', 'AND', 'OR', 'NOT' inside quotes are literals.
    QString sanitized = query.trimmed();
    sanitized.replace(QLatin1String("\""), QLatin1String("\"\""));
    sanitized = QLatin1String("\"") + sanitized + QLatin1String("\"*");

    QString sql = QStringLiteral(
        "SELECT m.id, m.folder_id, m.uid, m.message_id, m.thread_id, "
        "       m.subject, m.from_address, m.from_name, m.date_utc, "
        "       m.flags, m.size, m.body_fetched, m.to_list, m.cc_list, m.bcc_list, m.reply_to "
        "FROM messages m "
        "JOIN messages_fts ON m.id = messages_fts.rowid "
    );

    const bool needsFolderJoin = (accountId != -1);
    if (needsFolderJoin) {
        sql += QStringLiteral("JOIN folders f ON m.folder_id = f.id ");
    }

    sql += QStringLiteral("WHERE messages_fts MATCH ? ");

    if (folderId != -1) {
        sql += QStringLiteral("AND m.folder_id = ? ");
    } else if (accountId != -1) {
        sql += QStringLiteral("AND f.account_id = ? ");
    }

    sql += QStringLiteral("ORDER BY rank");

    QSqlQuery q(Database::connection());
    q.prepare(sql);
    q.addBindValue(sanitized);

    if (folderId != -1) {
        q.addBindValue(folderId);
    } else if (accountId != -1) {
        q.addBindValue(accountId);
    }

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("searchMessages failed for query '") + query + "': " + q.lastError().text());
        return {};
    }

    QList<Message> result;
    while (q.next()) {
        result.append(messageFromQuery(q));
    }
    return result;
}

