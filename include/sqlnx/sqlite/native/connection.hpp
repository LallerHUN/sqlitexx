#ifndef SQLNX_SQLITE_NATIVE_CONNECTION_HPP
#define SQLNX_SQLITE_NATIVE_CONNECTION_HPP

#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/native/collation_callback.hpp"
#include "sqlnx/sqlite/native/flags.hpp"
#include "sqlnx/sqlite/native/function_callbacks.hpp"
#include "sqlnx/sqlite/native/hook_callbacks.hpp"
#include "sqlnx/sqlite/native/result.hpp"
#include "sqlnx/sqlite/native/statement.hpp"

#include <cstdint>

extern "C"
{
    struct sqlite3;
}

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        /// A database connection (sqlite3). Move-only; the destructor closes it with sqlite3_close_v2, so statements
        /// may outlive their connection: the connection is released once its last statement is finalized.
        ///
        /// Mirrors the C API one to one: functions return result codes and never throw. Extended result codes are
        /// always switched on.
        class connection final
        {
        public:
            connection() noexcept = default;

            /// Takes ownership of a handle opened elsewhere.
            explicit connection(sqlite3* handle) noexcept;

            connection(connection&& other) noexcept;
            connection& operator=(connection&& other) noexcept;

            connection(const connection&) = delete;
            connection& operator=(const connection&) = delete;

            ~connection();

            /// \name Lifetime
            /// open() closes the current database first. When opening fails the handle is usually still set, so
            /// errmsg() can describe the failure; close() or the destructor releases it.
            ///@{
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
            ///@}

            /// \name Statements
            /// prepare() finalizes whatever `out` held. When `sql` holds only whitespace or comments, the
            /// result is ok and `out` stays empty. `tail` receives the unused rest of `sql`: a view into `sql`, so it
            /// is only null-terminated when `sql` is.
            ///@{
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
            ///@}

            /// Runs one or more semicolon-separated statements and discards their rows.
            result exec(const char* sql) noexcept;

            /// SQL functions (sqlite3_create_function_v2 with UTF-8 text). `argument_count` is -1 for any number of
            /// arguments. `destroy` (if not null) is called with `user_data` when SQLite no longer needs it: when the
            /// function is replaced, the connection closes, or this call fails.
            result create_function(
                const char* name,
                int argument_count,
                function_flags flags,
                void* user_data,
                function_callback function,
                destroy_callback destroy
            ) noexcept;

            /// Aggregate functions: `step` runs for every row of a group, `final` gives the group's result.
            result create_aggregate(
                const char* name,
                int argument_count,
                function_flags flags,
                void* user_data,
                function_callback step,
                final_callback final,
                destroy_callback destroy
            ) noexcept;

            /// Aggregates that also work as window functions (sqlite3_create_window_function): `value` gives the
            /// current result while the window moves, and `inverse` takes a row back out of it.
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

            /// Collations (sqlite3_create_collation_v2 with UTF-8 text), for COLLATE and ORDER BY. A null `compare`
            /// removes the collation. `destroy` (if not null) is called with `user_data` when the collation is replaced
            /// or removed or the connection closes. Unlike create_function, it is not called when this call fails: the
            /// caller still owns `user_data` then. Replacing or removing a collation while statements are running fails
            /// with busy.
            result create_collation(
                const char* name,
                void* user_data,
                compare_callback compare,
                destroy_callback destroy
            ) noexcept;

            /// Hooks: one of each per connection, each call replacing the one before. A null callback clears the hook.
            /// SQLite keeps `user_data` without owning it (there is no destroy callback); commit_hook, rollback_hook
            /// and update_hook return the previous one. A hook must not modify the connection that runs it, nor prepare
            /// or run statements on it. The progress handler runs every `instructions` virtual machine instructions of
            /// a statement (less than 1 turns it off).
            void* commit_hook(
                commit_callback hook,
                void* user_data
            ) noexcept;
            void* rollback_hook(
                rollback_callback hook,
                void* user_data
            ) noexcept;
            void* update_hook(
                update_callback hook,
                void* user_data
            ) noexcept;
            void progress_handler(
                int instructions,
                progress_callback handler,
                void* user_data
            ) noexcept;

            /// Errors of the most recent failed call on this connection
            const char* errmsg() const noexcept;
            result extended_errcode() const noexcept;

            /// \name State
            ///@{
            std::int64_t last_insert_rowid() const noexcept;
            std::int64_t changes() const noexcept;
            std::int64_t total_changes() const noexcept;
            bool autocommit() const noexcept;
            result busy_timeout(int milliseconds) noexcept;
            void interrupt() noexcept;
            ///@}

        private:
            sqlite3* db = nullptr;
        };

    } // native
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_NATIVE_CONNECTION_HPP
