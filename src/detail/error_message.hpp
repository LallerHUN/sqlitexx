#ifndef SQLITEXX_DETAIL_ERROR_MESSAGE_HPP
#define SQLITEXX_DETAIL_ERROR_MESSAGE_HPP

#include <sqlite3.h>

#include "sqlitexx/error.hpp"

namespace sqlitexx
{
namespace detail
{
    // The error for a failed call on `db`. The connection's message describes the failure only when its error code is
    // the one being reported; codes sqlitexx produced itself (for example a value out of range before SQLite was
    // called) get SQLite's generic description instead of a stale message from an earlier call.
    inline error make_error(
        sqlite3* db,
        native::result code
    )
    {
        if (db != nullptr && sqlite3_extended_errcode(db) == code.code())
        {
            return error(code, sqlite3_errmsg(db));
        }
        return error(code, code.errstr());
    }
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_ERROR_MESSAGE_HPP
