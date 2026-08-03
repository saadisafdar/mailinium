#include "storage/AccountStore.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QTimeZone>
#include <QVariant>

#include "storage/Database.h"

qint64 AccountStore::insertAccount(Account& account)
{
    if (!account.createdAt.isValid()) {
        account.createdAt = QDateTime::currentDateTimeUtc();
    }

    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts (email, display_name, password, provider, imap_host, imap_port, smtp_host, smtp_port, created_at, auth_type, access_token, refresh_token, token_expiry) "
        "VALUES (:email, :display_name, :password, :provider, :imap_host, :imap_port, :smtp_host, :smtp_port, :created_at, :auth_type, :access_token, :refresh_token, :token_expiry)"
    ));

    q.bindValue(QStringLiteral(":email"), account.email);
    q.bindValue(QStringLiteral(":display_name"), account.displayName);
    q.bindValue(QStringLiteral(":password"), account.password);
    q.bindValue(QStringLiteral(":provider"), account.provider);
    q.bindValue(QStringLiteral(":imap_host"), account.imapHost);
    q.bindValue(QStringLiteral(":imap_port"), account.imapPort);
    q.bindValue(QStringLiteral(":smtp_host"), account.smtpHost);
    q.bindValue(QStringLiteral(":smtp_port"), account.smtpPort);
    q.bindValue(QStringLiteral(":created_at"), account.createdAt.toString(Qt::ISODate));
    
    q.bindValue(QStringLiteral(":auth_type"), account.authType == AuthType::OAuth2 ? QStringLiteral("oauth2") : QStringLiteral("password"));
    q.bindValue(QStringLiteral(":access_token"), account.accessToken);
    q.bindValue(QStringLiteral(":refresh_token"), account.refreshToken);
    q.bindValue(QStringLiteral(":token_expiry"), account.tokenExpiry.isValid() ? account.tokenExpiry.toString(Qt::ISODate) : QStringLiteral(""));

    if (!q.exec()) {
        qWarning() << "ERROR [storage] insertAccount failed:" << q.lastError().text();
        return -1;
    }

    account.id = q.lastInsertId().toLongLong();
    return account.id;
}

bool AccountStore::updateAccount(const Account& account)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "UPDATE accounts SET "
        "email = :email, "
        "display_name = :display_name, "
        "password = :password, "
        "provider = :provider, "
        "imap_host = :imap_host, "
        "imap_port = :imap_port, "
        "smtp_host = :smtp_host, "
        "smtp_port = :smtp_port, "
        "auth_type = :auth_type, "
        "access_token = :access_token, "
        "refresh_token = :refresh_token, "
        "token_expiry = :token_expiry "
        "WHERE id = :id"
    ));

    q.bindValue(QStringLiteral(":email"), account.email);
    q.bindValue(QStringLiteral(":display_name"), account.displayName);
    q.bindValue(QStringLiteral(":password"), account.password);
    q.bindValue(QStringLiteral(":provider"), account.provider);
    q.bindValue(QStringLiteral(":imap_host"), account.imapHost);
    q.bindValue(QStringLiteral(":imap_port"), account.imapPort);
    q.bindValue(QStringLiteral(":smtp_host"), account.smtpHost);
    q.bindValue(QStringLiteral(":smtp_port"), account.smtpPort);
    
    q.bindValue(QStringLiteral(":auth_type"), account.authType == AuthType::OAuth2 ? QStringLiteral("oauth2") : QStringLiteral("password"));
    q.bindValue(QStringLiteral(":access_token"), account.accessToken);
    q.bindValue(QStringLiteral(":refresh_token"), account.refreshToken);
    q.bindValue(QStringLiteral(":token_expiry"), account.tokenExpiry.isValid() ? account.tokenExpiry.toString(Qt::ISODate) : QStringLiteral(""));

    q.bindValue(QStringLiteral(":id"), account.id);

    if (!q.exec()) {
        qWarning() << "ERROR [storage] updateAccount failed:" << q.lastError().text();
        return false;
    }

    return q.numRowsAffected() > 0;
}

bool AccountStore::deleteAccount(qint64 accountId)
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral("DELETE FROM accounts WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), accountId);

    if (!q.exec()) {
        qWarning() << "ERROR [storage] deleteAccount failed:" << q.lastError().text();
        return false;
    }

    return q.numRowsAffected() > 0;
}

std::optional<Account> AccountStore::accountById(qint64 id) const
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral(
        "SELECT id, email, display_name, password, provider, imap_host, imap_port, smtp_host, smtp_port, created_at, "
        "auth_type, access_token, refresh_token, token_expiry "
        "FROM accounts WHERE id = :id"
    ));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    Account acc;
    acc.id          = q.value(0).toLongLong();
    acc.email       = q.value(1).toString();
    acc.displayName = q.value(2).toString();
    acc.password    = q.value(3).toString();
    acc.provider    = q.value(4).toString();
    acc.imapHost    = q.value(5).toString();
    acc.imapPort    = q.value(6).toInt();
    acc.smtpHost    = q.value(7).toString();
    acc.smtpPort    = q.value(8).toInt();
    acc.createdAt   = QDateTime::fromString(q.value(9).toString(), Qt::ISODate);
    acc.createdAt.setTimeZone(QTimeZone::UTC);
    
    acc.authType     = (q.value(10).toString() == QLatin1String("oauth2")) ? AuthType::OAuth2 : AuthType::Password;
    acc.accessToken  = q.value(11).toString();
    acc.refreshToken = q.value(12).toString();
    
    QString expiryStr = q.value(13).toString();
    if (!expiryStr.isEmpty()) {
        acc.tokenExpiry = QDateTime::fromString(expiryStr, Qt::ISODate);
        acc.tokenExpiry.setTimeZone(QTimeZone::UTC);
    }

    return acc;
}

QList<Account> AccountStore::allAccounts() const
{
    QList<Account> results;
    QSqlQuery q(Database::connection());
    
    // Ordered by ID for consistent display order
    q.prepare(QStringLiteral(
        "SELECT id, email, display_name, password, provider, imap_host, imap_port, smtp_host, smtp_port, created_at, "
        "auth_type, access_token, refresh_token, token_expiry "
        "FROM accounts ORDER BY id ASC"
    ));

    if (!q.exec()) {
        qWarning() << "ERROR [storage] allAccounts failed:" << q.lastError().text();
        return results;
    }

    while (q.next()) {
        Account acc;
        acc.id          = q.value(0).toLongLong();
        acc.email       = q.value(1).toString();
        acc.displayName = q.value(2).toString();
        acc.password    = q.value(3).toString();
        acc.provider    = q.value(4).toString();
        acc.imapHost    = q.value(5).toString();
        acc.imapPort    = q.value(6).toInt();
        acc.smtpHost    = q.value(7).toString();
        acc.smtpPort    = q.value(8).toInt();
        acc.createdAt   = QDateTime::fromString(q.value(9).toString(), Qt::ISODate);
        acc.createdAt.setTimeZone(QTimeZone::UTC);

        acc.authType     = (q.value(10).toString() == QLatin1String("oauth2")) ? AuthType::OAuth2 : AuthType::Password;
        acc.accessToken  = q.value(11).toString();
        acc.refreshToken = q.value(12).toString();
        
        QString expiryStr = q.value(13).toString();
        if (!expiryStr.isEmpty()) {
            acc.tokenExpiry = QDateTime::fromString(expiryStr, Qt::ISODate);
            acc.tokenExpiry.setTimeZone(QTimeZone::UTC);
        }

        results.append(acc);
    }

    return results;
}

int AccountStore::accountCount() const
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM accounts"));
    
    if (!q.exec() || !q.next()) {
        qWarning() << "ERROR [storage] accountCount failed:" << q.lastError().text();
        return 0;
    }

    return q.value(0).toInt();
}

bool AccountStore::accountExists(const QString& email, const QString& provider) const
{
    QSqlQuery q(Database::connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM accounts WHERE email = :email AND provider = :provider"));
    q.bindValue(QStringLiteral(":email"), email);
    q.bindValue(QStringLiteral(":provider"), provider);
    
    if (!q.exec() || !q.next()) {
        return false;
    }
    
    return q.value(0).toInt() > 0;
}
