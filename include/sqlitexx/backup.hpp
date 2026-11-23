#ifndef SQLITEXX_BACKUP_HPP
#define SQLITEXX_BACKUP_HPP

// Copying a whole database into another one while it is in use (SQLite's online backup):
//
//     // Save an in-memory database to a file, all at once
//     expected<void> saved = memory.backup_to(file);
//
//     // Or in steps, with progress in between and other connections free to use the source
//     expected<backup> copy = big.begin_backup_to(file);
//     while (copy)
//     {
//         expected<bool> done = copy->step(100);
//         if (!done || *done) break;   // check done.error(): busy and locked can be retried
//         report(copy->page_count() - copy->remaining(), copy->page_count());
//     }
//
// The backup is a member of the database it copies from, so the direction reads from the call: a.backup_to(b)
// replaces b's content with a's. Restoring is the same call the other way round.

#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/backup.hpp"

extern "C"
{
    struct sqlite3;
}

namespace sqlitexx
{
class database;

// A backup in progress, from database::begin_backup_to. Move-only; the destructor ends it, keeping the pages copied so
// far. Both databases must outlive it.
class backup final
{
public:
    backup() noexcept = default;

    backup(backup&&) noexcept = default;
    backup& operator=(backup&&) noexcept = default;

    backup(const backup&) = delete;
    backup& operator=(const backup&) = delete;

    // Copies up to `pages` pages, or all that remain for a negative count. True when the copy is complete, false when
    // pages remain. A primary_code::busy or locked error means a lock was in the way: stepping again later retries.
    // Other errors end the backup.
    expected<bool> step(int pages = -1);

    // Progress, as of the last step(): pages left to copy, and pages in the source
    int remaining() const noexcept;
    int page_count() const noexcept;

    // The native backup underneath, for what the typed layer does not cover
    native::backup& native_backup() noexcept;

private:
    native::backup bak;
    sqlite3* destination = nullptr;

    friend class database;

    backup(
        native::backup&& started,
        sqlite3* destination_db
    ) noexcept;
};

} // sqlitexx

#endif // SQLITEXX_BACKUP_HPP
