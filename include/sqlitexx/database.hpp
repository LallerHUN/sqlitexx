#ifndef SQLITEXX_DATABASE_HPP
#define SQLITEXX_DATABASE_HPP

#include "sqlitexx/backup.hpp"
#include "sqlitexx/blob_stream.hpp"
#include "sqlitexx/config.hpp"
#include "sqlitexx/cstring_view.hpp"
#include "sqlitexx/detail/hook_slots.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/connection.hpp"
#include "sqlitexx/native/flags.hpp"
#include "sqlitexx/registrations.hpp"
#include "sqlitexx/statement.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace sqlitexx
{
// A database connection with a typed API. Move-only. Statements may outlive it (the native layer closes
// connections with sqlite3_close_v2).
class database final
{
public:
    database() noexcept = default;

    database(database&&) noexcept = default;
    database& operator=(database&& other) noexcept;

    database(const database&) = delete;
    database& operator=(const database&) = delete;

    // Lifetime
    static expected<database> open(
        const char* filename,
        native::open_flags flags = native::open_flags::default_rw
    ) noexcept;

    // Statements. execute() runs one or more semicolon-separated statements and discards their rows. prepare()
    // takes exactly one statement: SQL holding none, or several, is a primary_code::misuse error rather than
    // silently ignoring the rest.
    expected<void> execute(const char* sql) noexcept;
    expected<statement> prepare(string_view sql) noexcept;

    // State
    std::int64_t last_insert_rowid() const noexcept;
    std::int64_t changes() const noexcept;

    // Settings. When another connection holds a lock, retry for up to `timeout` before a statement fails with
    // primary_code::busy. Zero or less turns waiting off, as it is when a database opens. A timeout longer than SQLite
    // takes (INT_MAX milliseconds, about 24.8 days) waits that long. Coarser durations convert:
    // std::chrono::seconds(5).
    expected<void> busy_timeout(std::chrono::milliseconds timeout) noexcept;

    // Backup (see sqlitexx/backup.hpp): copies this database over `destination`, replacing everything in it.
    // backup_to() copies all at once; begin_backup_to() gives a backup to copy in steps. The schemas are "main",
    // "temp" or the name of an attached database.
    expected<void> backup_to(
        database& destination,
        cstring_view source_schema = "main",
        cstring_view destination_schema = "main"
    ) noexcept;
    expected<backup> begin_backup_to(
        database& destination,
        cstring_view source_schema = "main",
        cstring_view destination_schema = "main"
    ) noexcept;

    // Incremental blob I/O (see sqlitexx/blob_stream.hpp): opens the value in `column` of the row `rowid` of `table`
    // for reading and writing in pieces. The value must be a blob or text; NULL and numbers cannot be opened.
    expected<blob_stream> open_blob(
        cstring_view table,
        cstring_view column,
        std::int64_t rowid,
        blob_access access = blob_access::read_only,
        cstring_view schema = "main"
    ) noexcept;

    // SQL functions written in C++, with the parameters and return type of `function` (see sqlitexx/function.hpp).
    // Registering the same name with the same number of parameters again replaces the function.
    template <class F>
    expected<void> create_function(
        cstring_view name,
        F function,
        native::function_flags flags = native::function_flags::none
    ) noexcept(std::is_nothrow_move_constructible<F>::value);

    // Aggregate functions written in C++, from a class with step(...) and finish() members; with inverse(...) and
    // value() also a window function (see sqlitexx/aggregate.hpp). Each group works on its own copy of `prototype`.
    template <class Aggregate>
    expected<void> create_aggregate(
        cstring_view name,
        Aggregate prototype,
        native::function_flags flags = native::function_flags::none
    ) noexcept(std::is_nothrow_move_constructible<Aggregate>::value);

    // Collations written in C++, for COLLATE and ORDER BY: `compare` takes two string_views and returns a negative,
    // zero or positive number, or in C++20 what operator<=> gives (see sqlitexx/collation.hpp). Registering the same
    // name again replaces the collation.
    template <class Compare>
    expected<void> create_collation(
        cstring_view name,
        Compare compare
    ) noexcept(std::is_nothrow_move_constructible<Compare>::value);

    // The same, held by a registrations token: removed when the token ends, so the callable may refer to state that
    // ends with the token's scope (see sqlitexx/registrations.hpp)
    template <class F>
    expected<void> create_function(
        cstring_view name,
        F function,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    template <class F>
    expected<void> create_function(
        cstring_view name,
        F function,
        native::function_flags flags,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    template <class Aggregate>
    expected<void> create_aggregate(
        cstring_view name,
        Aggregate prototype,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<Aggregate>::value);
    template <class Aggregate>
    expected<void> create_aggregate(
        cstring_view name,
        Aggregate prototype,
        native::function_flags flags,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<Aggregate>::value);
    template <class Compare>
    expected<void> create_collation(
        cstring_view name,
        Compare compare,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<Compare>::value);

    // Removing what create_function, create_aggregate and create_collation registered. SQLite releases its copy of the
    // callable at once, so state the callable refers to may end after a successful call. A function is known by its
    // name and its number of parameters (an aggregate's: those of step()). Removing fails with primary_code::busy while
    // statements are running; removing what isn't registered succeeds and does nothing. Statements prepared earlier
    // are prepared again before their next step, and those using the name then fail.
    expected<void> remove_function(
        cstring_view name,
        int argument_count
    ) noexcept;
    expected<void> remove_collation(cstring_view name) noexcept;

    // Hooks (see sqlitexx/hooks.hpp): one of each per connection. Setting one replaces the one before, nullptr clears
    // it. commit_hook: called before a transaction commits; returning false turns the commit into a rollback.
    // rollback_hook: after a transaction rolls back. update_hook: for every row inserted, updated or deleted, with the
    // kind of change, the schema, the table and the rowid. progress_handler: every `instructions` virtual machine
    // instructions of a statement; returning false interrupts it. With a registrations token, the hook is cleared when
    // the token ends.
    template <class F>
    expected<void> commit_hook(F hook) noexcept(std::is_nothrow_move_constructible<F>::value);
    template <class F>
    expected<void> commit_hook(
        F hook,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    expected<void> commit_hook(std::nullptr_t) noexcept;

    template <class F>
    expected<void> rollback_hook(F hook) noexcept(std::is_nothrow_move_constructible<F>::value);
    template <class F>
    expected<void> rollback_hook(
        F hook,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    expected<void> rollback_hook(std::nullptr_t) noexcept;

    template <class F>
    expected<void> update_hook(F hook) noexcept(std::is_nothrow_move_constructible<F>::value);
    template <class F>
    expected<void> update_hook(
        F hook,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    expected<void> update_hook(std::nullptr_t) noexcept;

    template <class F>
    expected<void> progress_handler(
        int instructions,
        F handler
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    template <class F>
    expected<void> progress_handler(
        int instructions,
        F handler,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    expected<void> progress_handler(std::nullptr_t) noexcept;

    // The native connection underneath, for what the typed layer does not cover
    native::connection& native_connection() noexcept;
    const native::connection& native_connection() const noexcept;

private:
    native::connection db;
    // Created with the first hook. Declared after the connection: it clears the hooks before the connection closes.
    std::unique_ptr<detail::hook_slots> hooks;

    explicit database(native::connection&& opened) noexcept;

    error failure(native::result code) const noexcept;

    // The error for registering on a closed database
    static error needs_open(const char* message) noexcept;

    // Hooks: installing one made by Caller from `hook`, and clearing one
    template <
        class Caller,
        class F>
    expected<void> install_hook(
        F hook,
        int instructions,
        registrations* scope,
        const char* closed_message
    ) noexcept(std::is_nothrow_move_constructible<F>::value);
    expected<void> clear_hook(
        detail::registration_block::kind which,
        const char* closed_message
    ) noexcept;

    template <class Caller>
    native::result register_aggregate(
        cstring_view name,
        native::function_flags flags,
        void* prototype,
        std::false_type /*window*/
    ) noexcept;

    template <class Caller>
    native::result register_aggregate(
        cstring_view name,
        native::function_flags flags,
        void* prototype,
        std::true_type /*window*/
    ) noexcept;
};

} // sqlitexx

// create_function(), create_aggregate() and create_collation() are defined with the machinery that calls them
#include "sqlitexx/aggregate.hpp"
#include "sqlitexx/collation.hpp"
#include "sqlitexx/function.hpp"
#include "sqlitexx/hooks.hpp"

#endif // SQLITEXX_DATABASE_HPP
