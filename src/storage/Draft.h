#pragma once

#include <QDateTime>
#include <QString>

//
// Draft — represents one row in the `drafts` SQLite table.
//
struct Draft {
    qint64    id          = -1;
    qint64    accountId   = -1;
    QString   subject;
    QString   toList;
    QString   ccList;
    QString   bccList;
    QString   bodyHtml;
    QString   bodyPlain;
    QString   inReplyTo;  // Message-ID of the original message (for replies)
    QString   references; // Space-separated chain of Message-IDs
    QStringList attachmentPaths; // Absolute paths to local files
    QDateTime createdAt;
    QDateTime updatedAt;
};
