#include "impl_sqlitexx.hpp"

#include <cctype>
#include <chrono>
#include <limits>
#include <utility>

namespace sqlitexx
{
namespace
{
    bool only_whitespace(string_view text) noexcept
    {
        for (const char c : text)
        {
            if (!std::isspace(static_cast<unsigned char>(c)))
            {
                return false;
            }
        }
        return true;
    }
} // anonymous

database::database(native::connection&& opened) noexcept
    : db(std::move(opened))
{
}

expected<database> database::open(
    const char* filename,
    native::open_flags flags
)
{
    native::connection connection;
    const native::result code = connection.open(filename, flags);
    if (code.failed())
    {
        return expected<database>(unexpect, detail::make_error(connection.handle(), code));
    }
    return database(std::move(connection));
}

expected<void> database::execute(const char* sql)
{
    const native::result code = db.exec(sql);
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    return {};
}

expected<statement> database::prepare(string_view sql)
{
    native::statement prepared;
    string_view tail;
    const native::result code = db.prepare(sql, prepared, native::prepare_flags::none, &tail);
    if (code.failed())
    {
        return expected<statement>(unexpect, failure(code));
    }
    if (!prepared)
    {
        return expected<statement>(
            unexpect,
            error(native::result(SQLITE_MISUSE), "prepare() was given no SQL statement")
        );
    }

    // Whatever follows the first statement must be empty, whitespace or comments. Comments are recognised by
    // preparing the rest: SQLite gives no statement for them.
    if (!only_whitespace(tail))
    {
        native::statement rest;
        const native::result rest_code = db.prepare(tail, rest);
        if (rest_code.failed() || rest)
        {
            return expected<statement>(
                unexpect,
                error(native::result(SQLITE_MISUSE), "prepare() takes one SQL statement; use execute() to run several")
            );
        }
    }
    return statement(std::move(prepared));
}

std::int64_t database::last_insert_rowid() const noexcept
{
    return db.last_insert_rowid();
}

std::int64_t database::changes() const noexcept
{
    return db.changes();
}

expected<void> database::busy_timeout(std::chrono::milliseconds timeout)
{
    // The parentheses keep a max macro (<windows.h> without NOMINMAX) out of the call
    const std::chrono::milliseconds::rep longest = (std::numeric_limits<int>::max)();
    const std::chrono::milliseconds::rep wanted = timeout.count();
    const int milliseconds = wanted <= 0 ? 0 : static_cast<int>(wanted < longest ? wanted : longest);
    const native::result code = db.busy_timeout(milliseconds);
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    return {};
}

native::connection& database::native_connection() noexcept
{
    return db;
}

const native::connection& database::native_connection() const noexcept
{
    return db;
}

error database::failure(native::result code) const
{
    return detail::make_error(db.handle(), code);
}

} // sqlitexx
