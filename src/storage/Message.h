#pragma once

#include <QDateTime>
#include <QString>

//
// MessageFlags — bitmask constants for the `flags` column in the messages table.
//
// Mirrors the IMAP system flag set (RFC 3501 §2.3.2):
//   \Seen      the message has been read
//   \Flagged   user-starred / important
//   \Answered  a reply has been sent
//   \Deleted   marked for deletion (expunge pending)
//   \Draft     message is a draft
//
namespace MessageFlags {
    constexpr int Seen     = 1 << 0;
    constexpr int Flagged  = 1 << 1;
    constexpr int Answered = 1 << 2;
    constexpr int Deleted  = 1 << 3;
    constexpr int Draft    = 1 << 4;
}

//
// Message — represents one row from the `messages` table.
//
// This is the message envelope (header data). The body is fetched separately
// and stored in `body_parts` (added in Phase 3E). `bodyFetched` tracks
// whether the body is available locally.
//
// `date` is always stored and returned in UTC.
//
struct Message {
    qint64    id          = -1;
    qint64    folderId    = -1;
    qint64    uid         = 0;        // IMAP UID — unique within a folder
    QString   messageId;              // RFC 2822 Message-ID header value
    qint64    threadId    = 0;        // local thread grouping (assigned on ingest)
    QString   subject;
    QString   fromAddress;            // bare email address
    QString   fromName;               // display name from the From header
    QString   toList;                 // Comma separated list of To recipients
    QString   ccList;                 // Comma separated list of Cc recipients
    QString   bccList;                // Comma separated list of Bcc recipients
    QString   replyTo;                // Reply-To header value
    QDateTime date;                   // sent time, UTC
    int       flags       = 0;        // MessageFlags bitmask
    qint64    size        = 0;        // RFC 822 size in bytes
    bool      bodyFetched = false;
};
