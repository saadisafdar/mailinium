#pragma once

#include "Draft.h"

#include <optional>
#include <QList>

//
// DraftStore — creates, reads, updates and deletes drafts in the database.
//
// Requires Database::instance().open() to have been called.
//
class DraftStore
{
public:
    DraftStore() = default;

    // Insert a new draft. draft.id is set on success.
    // Returns the assigned id, or -1 on failure.
    qint64 insertDraft(Draft& draft);

    // Update an existing draft (matched by draft.id).
    // Returns true on success.
    bool updateDraft(const Draft& draft);

    // Delete a draft by id.
    bool deleteDraft(qint64 id);

    // Return all drafts for an account, newest-first.
    QList<Draft> draftsForAccount(qint64 accountId);

    // Find a draft by its id.
    std::optional<Draft> draftById(qint64 id);
};
