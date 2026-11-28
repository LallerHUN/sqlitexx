#ifndef SQLNX_SQLITE_DETAIL_ERROR_MESSAGE_HPP
#define SQLNX_SQLITE_DETAIL_ERROR_MESSAGE_HPP

#include <sqlite3.h>

#include "sqlnx/sqlite/detail/out_of_memory.hpp"
#include "sqlnx/sqlite/error.hpp"

#include <string>

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        // The error for a failed call on `db`. The connection's message describes the failure only when its error code
        // is the one being reported; codes sqlnx produced itself (for example a value out of range before SQLite was
        // called) get SQLite's generic description instead of a stale message from an earlier call. When copying the
        // message runs out of memory, the error keeps its code and goes without a message.
        inline error make_error(
            sqlite3* db,
            native::result code
        ) noexcept
        {
            const char* message =
                db != nullptr && sqlite3_extended_errcode(db) == code.code() ? sqlite3_errmsg(db) : code.errstr();
#if SQLNX_DETAIL_EXCEPTIONS
            try
            {
                return error(code, message);
            }
            catch (const std::bad_alloc&)
            {
                return error(code, std::string());
            }
#else
            return error(code, message);
#endif
        }
    } // detail
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_DETAIL_ERROR_MESSAGE_HPP
