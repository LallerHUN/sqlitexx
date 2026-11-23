#ifndef SQLITEXX_DATABASE_HPP
#define SQLITEXX_DATABASE_HPP

#include "sqlitexx/config.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/connection.hpp"
#include "sqlitexx/native/flags.hpp"
#include "sqlitexx/statement.hpp"

#include <cstdint>

namespace sqlitexx
{
// A database connection with a typed API. Move-only. Statements may outlive it (the native layer closes
// connections with sqlite3_close_v2).
class database final
{
public:
    database() noexcept = default;

    database(database&&) noexcept = default;
    database& operator=(database&&) noexcept = default;

    database(const database&) = delete;
    database& operator=(const database&) = delete;

    // Lifetime
    static expected<database> open(
        const char* filename,
        native::open_flags flags = native::open_flags::default_rw
    );

    // Statements. execute() runs one or more semicolon-separated statements and discards their rows. prepare()
    // takes exactly one statement: SQL holding none, or several, is a primary_code::misuse error rather than
    // silently ignoring the rest.
    expected<void> execute(const char* sql);
    expected<statement> prepare(string_view sql);

    // State
    std::int64_t last_insert_rowid() const noexcept;
    std::int64_t changes() const noexcept;

    // The native connection underneath, for what the typed layer does not cover
    native::connection& native_connection() noexcept;
    const native::connection& native_connection() const noexcept;

private:
    native::connection db;

    explicit database(native::connection&& opened) noexcept;

    error failure(native::result code) const;
};

} // sqlitexx

#endif // SQLITEXX_DATABASE_HPP
