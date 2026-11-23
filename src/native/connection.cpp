#include "impl_sqlitexx.hpp"

#include <climits>

namespace sqlitexx
{
namespace native
{
    connection::connection(sqlite3* handle) noexcept
        : db(handle)
    {
        if (db != nullptr)
        {
            sqlite3_extended_result_codes(db, 1);
        }
    }

    connection::connection(connection&& other) noexcept
        : db(other.release())
    {
    }

    connection& connection::operator=(connection&& other) noexcept
    {
        if (this != &other)
        {
            close();
            db = other.release();
        }
        return *this;
    }

    connection::~connection()
    {
        close();
    }

    result connection::open(
        const char* filename,
        open_flags flags,
        const char* vfs
    ) noexcept
    {
        close();
        const int rc = sqlite3_open_v2(filename, &db, static_cast<int>(flags) | SQLITE_OPEN_EXRESCODE, vfs);
        return result(rc);
    }

    result connection::open(const char16_t* filename) noexcept
    {
        close();
        const int rc = sqlite3_open16(filename, &db);
        if (db != nullptr)
        {
            sqlite3_extended_result_codes(db, 1);
        }
        return result(rc);
    }

    result connection::close() noexcept
    {
        // close_v2 never reports SQLITE_BUSY: with statements still open the connection becomes a zombie and is
        // released together with its last statement.
        const int rc = sqlite3_close_v2(db);
        db = nullptr;
        return result(rc);
    }

    sqlite3* connection::handle() const noexcept
    {
        return db;
    }

    sqlite3* connection::release() noexcept
    {
        sqlite3* handle = db;
        db = nullptr;
        return handle;
    }

    connection::operator bool() const noexcept
    {
        return db != nullptr;
    }

    result connection::prepare(
        string_view sql,
        statement& out,
        prepare_flags flags,
        string_view* tail
    ) noexcept
    {
        out.finalize();
        if (sql.size() > static_cast<std::size_t>(INT_MAX))
        {
            return result(SQLITE_TOOBIG);
        }

        sqlite3_stmt* handle = nullptr;
        const char* rest = nullptr;
        const int rc = sqlite3_prepare_v3(
            db,
            sql.data(),
            static_cast<int>(sql.size()),
            static_cast<unsigned int>(flags),
            &handle,
            &rest
        );
        out = statement(handle);

        if (tail != nullptr)
        {
            if (rest == nullptr)
            {
                *tail = string_view();
            }
            else
            {
                *tail = string_view(rest, sql.size() - static_cast<std::size_t>(rest - sql.data()));
            }
        }
        return result(rc);
    }

    result connection::prepare(
        u16string_view sql,
        statement& out,
        prepare_flags flags,
        u16string_view* tail
    ) noexcept
    {
        out.finalize();
        if (sql.size() > static_cast<std::size_t>(INT_MAX) / sizeof(char16_t))
        {
            return result(SQLITE_TOOBIG);
        }

        sqlite3_stmt* handle = nullptr;
        const void* rest = nullptr;
        const int rc = sqlite3_prepare16_v3(
            db,
            sql.data(),
            static_cast<int>(sql.size() * sizeof(char16_t)),
            static_cast<unsigned int>(flags),
            &handle,
            &rest
        );
        out = statement(handle);

        if (tail != nullptr)
        {
            if (rest == nullptr)
            {
                *tail = u16string_view();
            }
            else
            {
                const char16_t* rest16 = static_cast<const char16_t*>(rest);
                *tail = u16string_view(rest16, sql.size() - static_cast<std::size_t>(rest16 - sql.data()));
            }
        }
        return result(rc);
    }

    result connection::exec(const char* sql) noexcept
    {
        return result(sqlite3_exec(db, sql, nullptr, nullptr, nullptr));
    }

    const char* connection::errmsg() const noexcept
    {
        return sqlite3_errmsg(db);
    }

    result connection::extended_errcode() const noexcept
    {
        return result(sqlite3_extended_errcode(db));
    }

    std::int64_t connection::last_insert_rowid() const noexcept
    {
        return static_cast<std::int64_t>(sqlite3_last_insert_rowid(db));
    }

    std::int64_t connection::changes() const noexcept
    {
        return static_cast<std::int64_t>(sqlite3_changes64(db));
    }

    std::int64_t connection::total_changes() const noexcept
    {
        return static_cast<std::int64_t>(sqlite3_total_changes64(db));
    }

    bool connection::autocommit() const noexcept
    {
        return sqlite3_get_autocommit(db) != 0;
    }

    result connection::busy_timeout(int milliseconds) noexcept
    {
        return result(sqlite3_busy_timeout(db, milliseconds));
    }

    void connection::interrupt() noexcept
    {
        sqlite3_interrupt(db);
    }

    result connection::create_function(
        const char* name,
        int argument_count,
        function_flags flags,
        void* user_data,
        function_callback function,
        destroy_callback destroy
    ) noexcept
    {
        return result(sqlite3_create_function_v2(
            db,
            name,
            argument_count,
            SQLITE_UTF8 | static_cast<int>(flags),
            user_data,
            function,
            nullptr,
            nullptr,
            destroy
        ));
    }

} // native
} // sqlitexx
