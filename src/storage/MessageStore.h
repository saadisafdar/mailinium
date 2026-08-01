#pragma once

#include "Message.h"

#include <optional>
#include <QList>

//
// MessageStore — creates, reads, updates and deletes messages in the database.
//
// Requires Database::instance().open() to have been called before any method.
//
// Only header data (envelope) is managed here. Body content storage is added
// in Phase 3E when the message reader is implemented.
//
// Error handling follows the same convention as FolderStore: return false /
// -1 / empty optional on failure; errors are logged via Logger.
//
class MessageStore
{
public:
    MessageStore() = default;

    // Insert a new message envelope. `msg.id` is ignored on input.
    // Returns the assigned id on success, or -1 on failure.
    // Silently succeeds (returns existing id) if the (folder_id, uid) pair
    // already exists — duplicate delivery is not an error.
    qint64 insertMessage(const Message& msg);

    // Update the flags bitmask for a single message.
    bool updateFlags(qint64 messageId, int flags);

    // Mark the body as locally available (set body_fetched = 1).
    bool markBodyFetched(qint64 messageId);

    // Delete a single message.
    bool deleteMessage(qint64 messageId);

    // Delete all messages in a folder (used when re-syncing from scratch).
    bool deleteMessagesForFolder(qint64 folderId);

    // --- Queries -----------------------------------------------------------

    std::optional<Message> messageById(qint64 id);
    std::optional<Message> messageByUid(qint64 folderId, qint64 uid);

    // Returns messages for a folder, newest-first.
    // limit = 0 means no limit.
    QList<Message> messagesForFolder(qint64 folderId, int limit = 100, int offset = 0);

    int messageCount(qint64 folderId);
    int unreadCount(qint64 folderId);
};
