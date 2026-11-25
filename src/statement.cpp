#include "impl_sqlitexx.hpp"

#include <string>
#include <utility>

namespace sqlitexx
{
namespace
{
    char ascii_lower(char c) noexcept
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    // SQL identifiers match regardless of the case of ASCII letters (other characters must be equal)
    bool same_identifier(
        string_view name,
        const char* column
    ) noexcept
    {
        for (const char c : name)
        {
            if (*column == '\0' || ascii_lower(c) != ascii_lower(*column))
            {
                return false;
            }
            ++column;
        }
        return *column == '\0';
    }
} // anonymous

statement::statement(native::statement&& prepared) noexcept
    : stmt(std::move(prepared))
{
}

expected<bool> statement::step() noexcept
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

expected<void> statement::reset() noexcept
{
    return check(stmt.reset());
}

expected<int> statement::column_index(string_view name) const noexcept
{
    const int count = stmt.column_count();
    for (int index = 0; index < count; ++index)
    {
        const char* column = stmt.column_name(index);
        if (column == nullptr)
        {
            return expected<int>(
                unexpect,
                detail::built_error(
                    native::result(SQLITE_NOMEM),
                    [&]
                    {
                        return "out of memory reading the name of column " + std::to_string(index);
                    }
                )
            );
        }
        if (same_identifier(name, column))
        {
            return index;
        }
    }
    return expected<int>(unexpect, no_column(name));
}

native::statement& statement::native_statement() noexcept
{
    return stmt;
}

const native::statement& statement::native_statement() const noexcept
{
    return stmt;
}

error statement::failure(native::result code) const noexcept
{
    sqlite3* db = stmt.handle() == nullptr ? nullptr : sqlite3_db_handle(stmt.handle());
    return detail::make_error(db, code);
}

error statement::no_rows() const noexcept
{
    return detail::error_with(native::result(SQLITE_DONE), "the query produced no rows");
}

error statement::no_parameter(const char* name) const noexcept
{
    return detail::built_error(
        native::result(SQLITE_RANGE),
        [&]
        {
            return std::string("the statement has no parameter named ") + name;
        }
    );
}

error statement::no_column(string_view name) const noexcept
{
    return detail::built_error(
        native::result(SQLITE_RANGE),
        [&]
        {
            return "the statement has no column named " + std::string(name.data(), name.size());
        }
    );
}

error statement::null_column(int index) const noexcept
{
    const char* name = stmt.column_name(index);
    return detail::built_error(
        native::result(SQLITE_MISMATCH),
        [&]
        {
            const std::string column =
                name == nullptr ? std::to_string(index) : std::to_string(index) + " (" + name + ")";
            return "column " + column + " is NULL; read it as an optional to accept NULL";
        }
    );
}

void statement::restart() noexcept
{
    // reset() reports the error of the previous run, which its caller already received
    stmt.reset();
    stmt.clear_bindings();
}

} // sqlitexx
