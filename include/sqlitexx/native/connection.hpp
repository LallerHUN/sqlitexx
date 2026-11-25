#ifndef SQLITEXX_NATIVE_CONNECTION_HPP
#define SQLITEXX_NATIVE_CONNECTION_HPP

#include "sqlitexx/config.hpp"
#include "sqlitexx/native/flags.hpp"
#include "sqlitexx/native/function_callbacks.hpp"
#include "sqlitexx/native/result.hpp"
#include "sqlitexx/native/statement.hpp"

#include <cstdint>

extern "C"
{
    struct sqlite3;
}

namespace sqlitexx
{
namespace native
{
    // A database connection (sqlite3). Move-only; the destructor closes it with sqlite3_close_v2, so statements may
    // outlive their connection: the connection is released once its last statement is finalized.
    //
    // Mirrors the C API one to one: functions return result codes and never throw. Extended result codes are always
    // switched on.
    class connection final
    {
    public:
        connection() noexcept = default;

        // Takes ownership of a handle opened elsewhere.
        explicit connection(sqlite3* handle) noexcept;

        connection(connection&& other) noexcept;
        connection& operator=(connection&& other) noexcept;

        connection(const connection&) = delete;
        connection& operator=(const connection&) = delete;

        ~connection();

        // Lifetime. open() closes the current database first. When opening fails the handle is usually still set, so
        // errmsg() can describe the failure; close() or the destructor releases it.
        result open(
            const char* filename,
            open_flags flags = open_flags::default_rw,
            const char* vfs = nullptr
        ) noexcept;
        result open(const char16_t* filename) noexcept;
        result close() noexcept;
        sqlite3* handle() const noexcept;
        sqlite3* release() noexcept;
        explicit operator bool() const noexcept;

        // Statements. prepare() finalizes whatever `out` held. When `sql` holds only whitespace or comments, the
        // result is ok and `out` stays empty. `tail` receives the unused rest of `sql`: a view into `sql`, so it is
        // only null-terminated when `sql` is.
        result prepare(
            string_view sql,
            statement& out,
            prepare_flags flags = prepare_flags::none,
            string_view* tail = nullptr
        ) noexcept;

        result prepare(
            u16string_view sql,
            statement& out,
            prepare_flags flags = prepare_flags::none,
            u16string_view* tail = nullptr
        ) noexcept;

        // Runs one or more semicolon-separated statements and discards their rows.
        result exec(const char* sql) noexcept;

        // SQL functions (sqlite3_create_function_v2 with UTF-8 text). `argument_count` is -1 for any number of
        // arguments. `destroy` (if not null) is called with `user_data` when SQLite no longer needs it: when the
        // function is replaced, the connection closes, or this call fails.
        result create_function(
            const char* name,
            int argument_count,
            function_flags flags,
            void* user_data,
            function_callback function,
            destroy_callback destroy
        ) noexcept;

        // Aggregate functions: `step` runs for every row of a group, `final` gives the group's result.
        result create_aggregate(
            const char* name,
            int argument_count,
            function_flags flags,
            void* user_data,
            function_callback step,
            final_callback final,
            destroy_callback destroy
        ) noexcept;

        // Aggregates that also work as window functions (sqlite3_create_window_function): `value` gives the current
        // result while the window moves, and `inverse` takes a row back out of it.
        result create_window_function(
            const char* name,
            int argument_count,
            function_flags flags,
            void* user_data,
            function_callback step,
            final_callback final,
            final_callback value,
            function_callback inverse,
            destroy_callback destroy
        ) noexcept;

        // Errors of the most recent failed call on this connection
        const char* errmsg() const noexcept;
        result extended_errcode() const noexcept;

        // State
        std::int64_t last_insert_rowid() const noexcept;
        std::int64_t changes() const noexcept;
        std::int64_t total_changes() const noexcept;
        bool autocommit() const noexcept;
        result busy_timeout(int milliseconds) noexcept;
        void interrupt() noexcept;

    private:
        sqlite3* db = nullptr;
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_CONNECTION_HPP
