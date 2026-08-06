#include "storage/DraftStore.h"
#include "storage/Database.h"
#include "core/Logger.h"

#include <QSqlQuery>
#include <QSqlError>

// ---------------------------------------------------------------------------
// Internal helper: map a QSqlQuery row → Draft
// ---------------------------------------------------------------------------

static Draft draftFromQuery(const QSqlQuery& q)
{
    Draft d;
    d.id         = q.value(QStringLiteral("id")).toLongLong();
    d.accountId  = q.value(QStringLiteral("account_id")).toLongLong();
    d.subject    = q.value(QStringLiteral("subject")).toString();
    d.toList     = q.value(QStringLiteral("to_list")).toString();
    d.ccList     = q.value(QStringLiteral("cc_list")).toString();
    d.bccList    = q.value(QStringLiteral("bcc_list")).toString();
    d.bodyHtml   = q.value(QStringLiteral("body_html")).toString();
    d.bodyPlain  = q.value(QStringLiteral("body_plain")).toString();
    d.inReplyTo  = q.value(QStringLiteral("in_reply_to")).toString();
    d.references = q.value(QStringLiteral("references")).toString();
    QString atts = q.value(QStringLiteral("attachments")).toString();
    d.attachmentPaths = atts.isEmpty() ? QStringList() : atts.split(QLatin1Char('\x1F'));
    d.createdAt  = QDateTime::fromString(
                       q.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
    d.updatedAt  = QDateTime::fromString(
                       q.value(QStringLiteral("updated_at")).toString(), Qt::ISODate);
    return d;
}

// ---------------------------------------------------------------------------
// DraftStore::insertDraft
// ---------------------------------------------------------------------------

qint64 DraftStore::insertDraft(Draft& draft)
{
    QSqlQuery q(Database::connection());

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    q.prepare(QStringLiteral(
        "INSERT INTO drafts "
        "(account_id, subject, to_list, cc_list, bcc_list, "
        " body_html, body_plain, in_reply_to, \"references\", attachments, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    ));

    auto bindStr = [](const QString& s) { return s.isNull() ? QStringLiteral("") : s; };

    q.addBindValue(draft.accountId);
    q.addBindValue(bindStr(draft.subject));
    q.addBindValue(bindStr(draft.toList));
    q.addBindValue(bindStr(draft.ccList));
    q.addBindValue(bindStr(draft.bccList));
    q.addBindValue(bindStr(draft.bodyHtml));
    q.addBindValue(bindStr(draft.bodyPlain));
    q.addBindValue(bindStr(draft.inReplyTo));
    q.addBindValue(bindStr(draft.references));
    QString attsInsert = draft.attachmentPaths.join(QLatin1Char('\x1F'));
    q.addBindValue(bindStr(attsInsert));
    q.addBindValue(now);
    q.addBindValue(now);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("DraftStore::insertDraft failed: ") + q.lastError().text());
        return -1;
    }

    draft.id = q.lastInsertId().toLongLong();
    return draft.id;
}

// ---------------------------------------------------------------------------
// DraftStore::updateDraft
// ---------------------------------------------------------------------------

bool DraftStore::updateDraft(const Draft& draft)
{
    QSqlQuery q(Database::connection());

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    q.prepare(QStringLiteral(
        "UPDATE drafts SET "
        "account_id=?, subject=?, to_list=?, cc_list=?, bcc_list=?, "
        "body_html=?, body_plain=?, in_reply_to=?, \"references\"=?, attachments=?, updated_at=? "
        "WHERE id=?"
    ));

    auto bindStr = [](const QString& s) { return s.isNull() ? QStringLiteral("") : s; };

    q.addBindValue(draft.accountId);
    q.addBindValue(bindStr(draft.subject));
    q.addBindValue(bindStr(draft.toList));
    q.addBindValue(bindStr(draft.ccList));
    q.addBindValue(bindStr(draft.bccList));
    q.addBindValue(bindStr(draft.bodyHtml));
    q.addBindValue(bindStr(draft.bodyPlain));
    q.addBindValue(bindStr(draft.inReplyTo));
    q.addBindValue(bindStr(draft.references));
    QString attsUpdate = draft.attachmentPaths.join(QLatin1Char('\x1F'));
    q.addBindValue(bindStr(attsUpdate));
    q.addBindValue(now);
    q.addBindValue(draft.id);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("DraftStore::updateDraft failed: ") + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

// ---------------------------------------------------------------------------
// DraftStore::deleteDraft
// ---------------------------------------------------------------------------

bool DraftStore::deleteDraft(qint64 id)
{
    QSqlQuery q(Database::connection());

    q.prepare(QStringLiteral("DELETE FROM drafts WHERE id=?"));
    q.addBindValue(id);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("DraftStore::deleteDraft failed: ") + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

// ---------------------------------------------------------------------------
// DraftStore::draftsForAccount
// ---------------------------------------------------------------------------

QList<Draft> DraftStore::draftsForAccount(qint64 accountId)
{
    QSqlQuery q(Database::connection());

    q.prepare(QStringLiteral(
        "SELECT id, account_id, subject, to_list, cc_list, bcc_list, "
        "body_html, body_plain, in_reply_to, \"references\", attachments, created_at, updated_at "
        "FROM drafts WHERE account_id=? ORDER BY updated_at DESC"
    ));
    q.addBindValue(accountId);

    QList<Draft> list;
    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("db"),
            QStringLiteral("DraftStore::draftsForAccount failed: ") + q.lastError().text());
        return list;
    }

    while (q.next()) {
        list.append(draftFromQuery(q));
    }
    return list;
}

// ---------------------------------------------------------------------------
// DraftStore::draftById
// ---------------------------------------------------------------------------

std::optional<Draft> DraftStore::draftById(qint64 id)
{
    QSqlQuery q(Database::connection());

    q.prepare(QStringLiteral(
        "SELECT id, account_id, subject, to_list, cc_list, bcc_list, "
        "body_html, body_plain, in_reply_to, \"references\", attachments, created_at, updated_at "
        "FROM drafts WHERE id=?"
    ));
    q.addBindValue(id);

    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    return draftFromQuery(q);
}
