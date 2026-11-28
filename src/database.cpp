#include "impl_sqlnx.hpp"

#include <cctype>
#include <chrono>
#include <limits>
#include <utility>

namespace sqlnx
{
namespace sqlite
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
    ) noexcept
    {
        native::connection connection;
        const native::result code = connection.open(filename, flags);
        if (code.failed())
        {
            return expected<database>(unexpect, detail::make_error(connection.handle(), code));
        }
        return database(std::move(connection));
    }

    expected<void> database::execute(const char* sql) noexcept
    {
        const native::result code = db.exec(sql);
        if (code.failed())
        {
            return expected<void>(unexpect, failure(code));
        }
        return {};
    }

    expected<statement> database::prepare(string_view sql) noexcept
    {
        return prepare_with(sql, native::prepare_flags::none);
    }

    expected<statement> database::prepare_with(
        string_view sql,
        native::prepare_flags flags
    ) noexcept
    {
        native::statement prepared;
        string_view tail;
        const native::result code = db.prepare(sql, prepared, flags, &tail);
        if (code.failed())
        {
            return expected<statement>(unexpect, failure(code));
        }
        if (!prepared)
        {
            return expected<statement>(
                unexpect,
                detail::error_with(native::result(SQLITE_MISUSE), "prepare() was given no SQL statement")
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
                    detail::error_with(
                        native::result(SQLITE_MISUSE),
                        "prepare() takes one SQL statement; use execute() to run several"
                    )
                );
            }
        }
        return statement(std::move(prepared));
    }

    void database::statement_cache_size(std::size_t statements) noexcept
    {
        cache_capacity = statements;
        if (cache)
        {
            cache->resize(statements);
        }
    }

    std::size_t database::statement_cache_size() const noexcept
    {
        return cache_capacity;
    }

    detail::lent_statement database::lend(string_view sql) noexcept
    {
        if (cache_capacity == 0)
        {
            return detail::lent_statement{prepare(sql), detail::cache_lease()};
        }
        // Running out of memory for the cache or its key only means the statement isn't kept
#if SQLNX_DETAIL_EXCEPTIONS
        if (!cache)
        {
            try
            {
                cache = std::make_shared<detail::statement_cache>(cache_capacity);
            }
            catch (const std::bad_alloc&)
            {
                return detail::lent_statement{prepare(sql), detail::cache_lease()};
            }
        }
#else
        if (!cache)
        {
            cache = std::make_shared<detail::statement_cache>(cache_capacity);
        }
#endif

        std::string key;
        statement taken;
        if (cache->take(sql, key, taken))
        {
            return detail::lent_statement{
                expected<statement>(std::move(taken)),
                detail::cache_lease(cache, std::move(key))
            };
        }

        expected<statement> prepared = prepare_with(sql, native::prepare_flags::persistent);
        if (!prepared)
        {
            return detail::lent_statement{std::move(prepared), detail::cache_lease()};
        }
#if SQLNX_DETAIL_EXCEPTIONS
        try
        {
            key.assign(sql.data(), sql.size());
        }
        catch (const std::bad_alloc&)
        {
            return detail::lent_statement{std::move(prepared), detail::cache_lease()};
        }
#else
        key.assign(sql.data(), sql.size());
#endif
        return detail::lent_statement{std::move(prepared), detail::cache_lease(cache, std::move(key))};
    }

    std::int64_t database::last_insert_rowid() const noexcept
    {
        return db.last_insert_rowid();
    }

    std::int64_t database::changes() const noexcept
    {
        return db.changes();
    }

    expected<void> database::busy_timeout(std::chrono::milliseconds timeout) noexcept
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

    expected<void> database::backup_to(
        database& destination,
        cstring_view source_schema,
        cstring_view destination_schema
    ) noexcept
    {
        expected<backup> started = begin_backup_to(destination, source_schema, destination_schema);
        if (!started)
        {
            return expected<void>(unexpect, std::move(started.error()));
        }
        // A negative count copies every page in one step
        expected<bool> done = started->step(-1);
        if (!done)
        {
            return expected<void>(unexpect, std::move(done.error()));
        }
        return {};
    }

    expected<backup> database::begin_backup_to(
        database& destination,
        cstring_view source_schema,
        cstring_view destination_schema
    ) noexcept
    {
        if (!db || !destination.db)
        {
            return expected<backup>(
                unexpect,
                detail::error_with(native::result(SQLITE_MISUSE), "a backup needs both databases open")
            );
        }
        native::backup started;
        const native::result code = started.init(destination.db, destination_schema.c_str(), db, source_schema.c_str());
        if (code.failed())
        {
            return expected<backup>(unexpect, detail::make_error(destination.db.handle(), code));
        }
        return backup(std::move(started), destination.db.handle());
    }

    expected<blob_stream> database::open_blob(
        cstring_view table,
        cstring_view column,
        std::int64_t rowid,
        blob_access access,
        cstring_view schema
    ) noexcept
    {
        if (!db)
        {
            return expected<blob_stream>(
                unexpect,
                detail::error_with(native::result(SQLITE_MISUSE), "open_blob() needs an open database")
            );
        }
        native::blob_handle opened;
        const native::result code =
            opened.open(db, schema.c_str(), table.c_str(), column.c_str(), rowid, access == blob_access::read_write);
        if (code.failed())
        {
            return expected<blob_stream>(unexpect, failure(code));
        }
        return blob_stream(std::move(opened), db.handle());
    }

    native::connection& database::native_connection() noexcept
    {
        return db;
    }

    const native::connection& database::native_connection() const noexcept
    {
        return db;
    }

    expected<void> database::remove_function(
        cstring_view name,
        int argument_count
    ) noexcept
    {
        if (!db)
        {
            return expected<void>(
                unexpect,
                detail::error_with(native::result(SQLITE_MISUSE), "remove_function() needs an open database")
            );
        }
        // A function without callbacks removes the one registered under the name and count
        const native::result code =
            db.create_function(name.c_str(), argument_count, native::function_flags::none, nullptr, nullptr, nullptr);
        if (code.failed())
        {
            return expected<void>(unexpect, failure(code));
        }
        return {};
    }

    expected<void> database::remove_collation(cstring_view name) noexcept
    {
        if (!db)
        {
            return expected<void>(
                unexpect,
                detail::error_with(native::result(SQLITE_MISUSE), "remove_collation() needs an open database")
            );
        }
        // A collation without a comparison removes the one registered under the name
        const native::result code = db.create_collation(name.c_str(), nullptr, nullptr, nullptr);
        if (code.failed())
        {
            return expected<void>(unexpect, failure(code));
        }
        return {};
    }

    database& database::operator=(database&& other) noexcept
    {
        if (this != &other)
        {
            // The hooks and the cached statements go first, while their connection is still open
            hooks.reset();
            cache.reset();
            db = std::move(other.db);
            hooks = std::move(other.hooks);
            cache = std::move(other.cache);
            cache_capacity = other.cache_capacity;
        }
        return *this;
    }

    expected<void> database::commit_hook(std::nullptr_t) noexcept
    {
        return clear_hook(detail::registration_block::kind::commit_hook, "commit_hook() needs an open database");
    }

    expected<void> database::rollback_hook(std::nullptr_t) noexcept
    {
        return clear_hook(detail::registration_block::kind::rollback_hook, "rollback_hook() needs an open database");
    }

    expected<void> database::update_hook(std::nullptr_t) noexcept
    {
        return clear_hook(detail::registration_block::kind::update_hook, "update_hook() needs an open database");
    }

    expected<void> database::progress_handler(std::nullptr_t) noexcept
    {
        return clear_hook(
            detail::registration_block::kind::progress_handler,
            "progress_handler() needs an open database"
        );
    }

    expected<void> database::clear_hook(
        detail::registration_block::kind which,
        const char* closed_message
    ) noexcept
    {
        if (!db)
        {
            return expected<void>(unexpect, needs_open(closed_message));
        }
        if (hooks)
        {
            hooks->clear(which);
        }
        return {};
    }

    error database::needs_open(const char* message) noexcept
    {
        return detail::error_with(native::result(SQLITE_MISUSE), message);
    }

    error database::failure(native::result code) const noexcept
    {
        return detail::make_error(db.handle(), code);
    }

} // sqlite
} // sqlnx
