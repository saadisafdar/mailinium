#include "FolderStore.h"
#include "Database.h"
#include "../core/Logger.h"

#include <QSqlError>
#include <QSqlQuery>

// ---------------------------------------------------------------------------
// Helper: map one QSqlQuery row to a Folder
// ---------------------------------------------------------------------------

static Folder folderFromQuery(const QSqlQuery& q)
{
    Folder f;
    f.id            = q.value(0).toLongLong();
    f.accountId     = q.value(1).toLongLong();
    f.name          = q.value(2).toString();
    f.role          = q.value(3).toString();
    f.uidvalidity   = q.value(4).toLongLong();
    f.highestModseq = q.value(5).toLongLong();
    return f;
}

// Column order used throughout:
//   0: id  1: account_id  2: name  3: role  4: uidvalidity  5: highestmodseq
static constexpr auto kSelectColumns =
    "SELECT id, account_id, name, role, uidvalidity, highestmodseq FROM folders";

// ---------------------------------------------------------------------------
// Insert
// ---------------------------------------------------------------------------

qint64 FolderStore::insertFolder(const Folder& folder)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO folders(account_id, name, role, uidvalidity, highestmodseq)"
        "    VALUES(:accountId, :name, :role, :uidvalidity, :highestmodseq)"
    ));
    q.bindValue(QStringLiteral(":accountId"),     folder.accountId);
    q.bindValue(QStringLiteral(":name"),          folder.name);
    q.bindValue(QStringLiteral(":role"),          folder.role);
    q.bindValue(QStringLiteral(":uidvalidity"),   folder.uidvalidity);
    q.bindValue(QStringLiteral(":highestmodseq"), folder.highestModseq);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("insertFolder failed: ") + q.lastError().text());
        return -1;
    }

    return q.lastInsertId().toLongLong();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

bool FolderStore::updateSyncState(qint64 folderId,
                                  qint64 uidvalidity,
                                  qint64 highestModseq)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "UPDATE folders"
        "    SET uidvalidity = :uv, highestmodseq = :hm"
        "    WHERE id = :id"
    ));
    q.bindValue(QStringLiteral(":uv"), uidvalidity);
    q.bindValue(QStringLiteral(":hm"), highestModseq);
    q.bindValue(QStringLiteral(":id"), folderId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("updateSyncState failed for folder ")
            + QString::number(folderId) + QStringLiteral(": ")
            + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

// ---------------------------------------------------------------------------
// Delete
// ---------------------------------------------------------------------------

bool FolderStore::deleteFolder(qint64 folderId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral("DELETE FROM folders WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), folderId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("deleteFolder failed for id ")
            + QString::number(folderId) + QStringLiteral(": ")
            + q.lastError().text());
        return false;
    }

    return q.numRowsAffected() > 0;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::optional<Folder> FolderStore::folderById(qint64 id)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "%1 WHERE id = :id"
    ).arg(QString::fromLatin1(kSelectColumns)));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("folderById failed: ") + q.lastError().text());
        return std::nullopt;
    }

    if (!q.next()) {
        return std::nullopt;
    }

    return folderFromQuery(q);
}

std::optional<Folder> FolderStore::folderByName(qint64 accountId,
                                                 const QString& name)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "%1 WHERE account_id = :aid AND name = :name"
    ).arg(QString::fromLatin1(kSelectColumns)));
    q.bindValue(QStringLiteral(":aid"),  accountId);
    q.bindValue(QStringLiteral(":name"), name);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("folderByName failed: ") + q.lastError().text());
        return std::nullopt;
    }

    if (!q.next()) {
        return std::nullopt;
    }

    return folderFromQuery(q);
}

std::optional<Folder> FolderStore::folderByRole(qint64 accountId,
                                                 const QString& role)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "%1 WHERE account_id = :aid AND role = :role"
    ).arg(QString::fromLatin1(kSelectColumns)));
    q.bindValue(QStringLiteral(":aid"),  accountId);
    q.bindValue(QStringLiteral(":role"), role);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("folderByRole failed: ") + q.lastError().text());
        return std::nullopt;
    }

    if (!q.next()) {
        return std::nullopt;
    }

    return folderFromQuery(q);
}

QList<Folder> FolderStore::foldersForAccount(qint64 accountId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "%1 WHERE account_id = :aid ORDER BY name ASC"
    ).arg(QString::fromLatin1(kSelectColumns)));
    q.bindValue(QStringLiteral(":aid"), accountId);

    if (!q.exec()) {
        Logger::instance().error(QStringLiteral("storage"),
            QStringLiteral("foldersForAccount failed: ") + q.lastError().text());
        return {};
    }

    QList<Folder> result;
    while (q.next()) {
        result.append(folderFromQuery(q));
    }

    return result;
}
