#include "impl_sqlitexx.hpp"

#include <string>
#include <utility>

namespace sqlitexx
{
statement::statement(native::statement&& prepared) noexcept
    : stmt(std::move(prepared))
{
}

expected<bool> statement::step()
{
    const native::result code = stmt.step();
    if (code.row())
    {
        return true;
    }
    if (code.done())
    {
        return false;
    }
    return expected<bool>(unexpect, failure(code));
}

expected<void> statement::reset()
{
    return check(stmt.reset());
}

native::statement& statement::native_statement() noexcept
{
    return stmt;
}

const native::statement& statement::native_statement() const noexcept
{
    return stmt;
}

error statement::failure(native::result code) const
{
    sqlite3* db = stmt.handle() == nullptr ? nullptr : sqlite3_db_handle(stmt.handle());
    return detail::make_error(db, code);
}

error statement::no_rows() const
{
    return error(native::result(SQLITE_DONE), "the query produced no rows");
}

error statement::no_parameter(const char* name) const
{
    return error(native::result(SQLITE_RANGE), std::string("the statement has no parameter named ") + name);
}

error statement::null_column(int index) const
{
    return error(
        native::result(SQLITE_MISMATCH),
        "column " + std::to_string(index) + " is NULL; read it as an optional to accept NULL"
    );
}

void statement::restart() noexcept
{
    // reset() reports the error of the previous run, which its caller already received
    stmt.reset();
    stmt.clear_bindings();
}

} // sqlitexx
