#include "impl_sqlitexx.hpp"

namespace sqlitexx
{
namespace
{
    const char* begin_sql(transaction_mode mode) noexcept
    {
        switch (mode)
        {
        case transaction_mode::immediate:
            return "BEGIN IMMEDIATE";
        case transaction_mode::exclusive:
            return "BEGIN EXCLUSIVE";
        case transaction_mode::deferred:
            break;
        }
        return "BEGIN DEFERRED";
    }
} // anonymous

transaction::transaction(sqlite3* handle) noexcept
    : db(handle)
{
}

transaction::transaction(transaction&& other) noexcept
    : db(other.db)
{
    other.db = nullptr;
}

transaction& transaction::operator=(transaction&& other) noexcept
{
    if (this != &other)
    {
        rollback_if_open();
        db = other.db;
        other.db = nullptr;
    }
    return *this;
}

transaction::~transaction()
{
    rollback_if_open();
}

expected<transaction> transaction::begin(
    database& target,
    transaction_mode mode
)
{
    sqlite3* handle = target.native_connection().handle();
    if (handle == nullptr)
    {
        return expected<transaction>(unexpect, error(native::result(SQLITE_MISUSE), "the database is not open"));
    }

    const native::result code(sqlite3_exec(handle, begin_sql(mode), nullptr, nullptr, nullptr));
    if (code.failed())
    {
        return expected<transaction>(unexpect, detail::make_error(handle, code));
    }
    return transaction(handle);
}

expected<void> transaction::commit()
{
    return finish("COMMIT");
}

expected<void> transaction::rollback()
{
    return finish("ROLLBACK");
}

bool transaction::active() const noexcept
{
    return db != nullptr;
}

expected<void> transaction::finish(const char* sql)
{
    if (db == nullptr)
    {
        return expected<void>(unexpect, error(native::result(SQLITE_MISUSE), "the transaction has already ended"));
    }

    const native::result code(sqlite3_exec(db, sql, nullptr, nullptr, nullptr));
    if (code.failed())
    {
        error failure = detail::make_error(db, code);

        // Some errors make SQLite roll the transaction back itself; back in autocommit mode means it has ended
        if (sqlite3_get_autocommit(db) != 0)
        {
            db = nullptr;
        }
        return expected<void>(unexpect, failure);
    }

    db = nullptr;
    return {};
}

void transaction::rollback_if_open() noexcept
{
    // Only while a transaction is really open: the user may have ended it with plain SQL meanwhile
    if (db != nullptr && sqlite3_get_autocommit(db) == 0)
    {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    db = nullptr;
}

} // sqlitexx
