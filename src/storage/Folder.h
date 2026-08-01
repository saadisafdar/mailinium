#pragma once

#include <QString>

//
// Folder — represents one row from the `folders` table.
//
// A folder belongs to one account and maps to an IMAP mailbox.
// The `role` field identifies well-known system folders:
//   "inbox" | "sent" | "drafts" | "trash" | "spam" | "archive" | "" (custom)
//
// `uidvalidity` and `highestModseq` are IMAP sync state values kept here
// to avoid a separate sync_state table join on every sync tick.
//
struct Folder {
    qint64  id            = -1;
    qint64  accountId     = -1;
    QString name;
    QString role;
    qint64  uidvalidity   = 0;
    qint64  highestModseq = 0;
};
