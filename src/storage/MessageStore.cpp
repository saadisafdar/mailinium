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
    return m;
}

// Column order used throughout — update messageFromQuery() if this changes.
static constexpr auto kSelectColumns =
    "SELECT id, folder_id, uid, message_id, thread_id,"
    "       subject, from_address, from_name, date_utc,"
    "       flags, size, body_fetched"
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
        "     flags, size, body_fetched)"
        "    VALUES(:folderId, :uid, :messageId, :threadId,"
        "           :subject, :fromAddress, :fromName, :dateUtc,"
        "           :flags, :size, :bodyFetched)"
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
    QString sql = QStringLiteral(
        "%1 WHERE folder_id = :fid"
        "   ORDER BY date_utc DESC")
        .arg(QString::fromLatin1(kSelectColumns));

    if (limit > 0) {
        sql += QStringLiteral(" LIMIT %1 OFFSET %2")
               .arg(limit).arg(offset);
    }

    QSqlQuery q(Database::connection());
    q.prepare(sql);
    q.bindValue(QStringLiteral(":fid"), folderId);

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
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM messages WHERE folder_id = :fid"));
    q.bindValue(QStringLiteral(":fid"), folderId);

    if (!q.exec() || !q.next()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("messageCount failed: ") + q.lastError().text());
        return 0;
    }

    return q.value(0).toInt();
}

int MessageStore::unreadCount(qint64 folderId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM messages"
        "    WHERE folder_id = :fid AND (flags & :seenFlag) = 0"));
    q.bindValue(QStringLiteral(":fid"),      folderId);
    q.bindValue(QStringLiteral(":seenFlag"), MessageFlags::Seen);

    if (!q.exec() || !q.next()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("unreadCount failed: ") + q.lastError().text());
        return 0;
    }

    return q.value(0).toInt();
}
