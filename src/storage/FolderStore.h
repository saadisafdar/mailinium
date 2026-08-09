#pragma once

#include "Folder.h"

#include <optional>
#include <QList>

//
// FolderStore — creates, reads, updates and deletes folders in the database.
//
// Requires Database::instance().open() to have been called before any method.
//
// All methods execute synchronously on the calling thread. For Phase 3 this
// is always the UI thread; a background-thread variant is added in Phase 3D.
//
// Error handling: methods return false / -1 / empty optional on failure and
// log the SQL error via Logger. Callers do not need to inspect QSqlError.
//
class FolderStore
{
public:
    FolderStore() = default;

    // Insert a new folder. `folder.id` is ignored on input.
    // Returns the assigned id on success, or -1 on failure.
    qint64 insertFolder(const Folder& folder);

    // Update the sync state fields (uidvalidity, highestModseq) of an
    // existing folder. Only those two fields are updated — name and role
    // are set once on creation and never change in normal operation.
    bool updateSyncState(qint64 folderId, qint64 uidvalidity, qint64 highestModseq);

    // Delete a folder and all its messages (via ON DELETE CASCADE).
    bool deleteFolder(qint64 folderId);

    // Look up a single folder by its primary key.
    std::optional<Folder> folderById(qint64 id);

    // Look up a single folder by its IMAP name within an account.
    // Returns nullopt if not found.
    std::optional<Folder> folderByName(qint64 accountId, const QString& name);

    // Look up a single folder by its role within an account.
    // Returns nullopt if not found.
    std::optional<Folder> folderByRole(qint64 accountId, const QString& role);

    // Returns all folders belonging to an account, ordered by name.
    QList<Folder> foldersForAccount(qint64 accountId);
};
