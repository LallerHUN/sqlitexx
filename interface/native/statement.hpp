#ifndef SQLITEXX_NATIVE_STATEMENT_HPP
#define SQLITEXX_NATIVE_STATEMENT_HPP

#include "sqlite_symbols.hpp"

namespace sqlitexx
{
namespace native
{

    struct statement final
    {
        int step();
        int reset();
        int finalize();

        statement() = default;
        ~statement();

    private:
        statement(sqlite3_stmt* handle);

        sqlite3_stmt* handle = nullptr;

        friend struct connection;
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_STATEMENT_HPP
