#ifndef SQLITEXX_NATIVE_BACKUP_HPP
#define SQLITEXX_NATIVE_BACKUP_HPP

#include "sqlitexx/native/connection.hpp"
#include "sqlitexx/native/result.hpp"

extern "C"
{
    struct sqlite3_backup;
}

namespace sqlitexx
{
namespace native
{
    // An online backup (sqlite3_backup): copies one database into another, page by page. Move-only; the destructor
    // finishes it. Both connections must outlive it.
    //
    // Mirrors the C API one to one: functions return result codes and never throw.
    class backup final
    {
    public:
        backup() noexcept = default;

        // Takes ownership of a handle started elsewhere.
        explicit backup(sqlite3_backup* handle) noexcept;

        backup(backup&& other) noexcept;
        backup& operator=(backup&& other) noexcept;

        backup(const backup&) = delete;
        backup& operator=(const backup&) = delete;

        ~backup();

        // Lifetime. init() finishes whatever this held, then prepares to copy the database `source_name` of `source`
        // ("main", "temp" or an attached name) over `destination_name` of `destination`. When it fails, nothing is
        // held and the result is destination's error code; destination.errmsg() describes it.
        result init(
            connection& destination,
            const char* destination_name,
            connection& source,
            const char* source_name
        ) noexcept;
        result finish() noexcept;
        sqlite3_backup* handle() const noexcept;
        sqlite3_backup* release() noexcept;
        explicit operator bool() const noexcept;

        // Copying. step() copies up to `pages` pages (all of them for a negative count): done when it has finished,
        // ok when pages remain, busy or locked when it can be retried later, another code when the backup failed.
        result step(int pages) noexcept;
        int remaining() const noexcept;
        int pagecount() const noexcept;

    private:
        sqlite3_backup* bak = nullptr;
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_BACKUP_HPP
