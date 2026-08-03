#pragma once

#include <QList>
#include <optional>

#include "storage/Account.h"

//
// AccountStore — CRUD operations for the `accounts` table.
//
// By design, instances of this class are cheap to construct and use
// the default global QSqlDatabase connection. They should be created
// locally on the stack when needed rather than kept around.
//
class AccountStore
{
public:
    AccountStore() = default;

    // Inserts a new account into the database.
    // Returns the newly generated ID on success, or -1 on failure.
    // The createdAt field will be populated with the current UTC time if not set.
    qint64 insertAccount(Account& account);

    // Updates an existing account's details.
    // Returns true on success, false on failure (e.g., account not found).
    bool updateAccount(const Account& account);

    // Deletes an account and all associated folders and messages (due to ON DELETE CASCADE).
    // Returns true if a row was deleted, false otherwise.
    bool deleteAccount(qint64 accountId);

    // Retrieves an account by its unique ID.
    std::optional<Account> accountById(qint64 id) const;

    // Retrieves all configured accounts, ordered by ID ascending.
    QList<Account> allAccounts() const;

    // Returns the total number of configured accounts.
    int accountCount() const;
    
    // Checks if an account with the given email and provider already exists.
    bool accountExists(const QString& email, const QString& provider) const;
};
