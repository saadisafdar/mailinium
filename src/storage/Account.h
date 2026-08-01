#pragma once

#include <QDateTime>
#include <QString>

//
// Account — represents a single email account configured in the application.
// Corresponds to a row in the `accounts` SQLite table.
//
struct Account {
    qint64    id = -1;
    QString   email = QStringLiteral("");
    QString   displayName = QStringLiteral("");
    QString   provider = QStringLiteral("");
    QString   imapHost = QStringLiteral("");
    int       imapPort = 993;
    QString   smtpHost = QStringLiteral("");
    int       smtpPort = 587;
    QDateTime createdAt;
};
