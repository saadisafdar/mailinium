#pragma once

#include <QDateTime>
#include <QString>

//
// AuthType — the authentication mechanism for the account.
//
enum class AuthType {
    Password,
    OAuth2
};

//
// Account — represents a single email account configured in the application.
// Corresponds to a row in the `accounts` SQLite table.
//
struct Account {
    qint64    id = -1;
    QString   email = QStringLiteral("");
    QString   displayName = QStringLiteral("");
    QString   password = QStringLiteral("");
    QString   provider = QStringLiteral("");
    QString   imapHost = QStringLiteral("");
    int       imapPort = 993;
    QString   smtpHost = QStringLiteral("");
    int       smtpPort = 587;
    QDateTime createdAt;

    // OAuth 2.0 fields
    AuthType  authType = AuthType::Password;
    QString   accessToken = QStringLiteral("");
    QString   refreshToken = QStringLiteral("");
    QDateTime tokenExpiry;
};
