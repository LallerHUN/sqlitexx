#ifndef SQLNX_SQLITE_DETAIL_TEXT_AND_BLOB_HPP
#define SQLNX_SQLITE_DETAIL_TEXT_AND_BLOB_HPP

#include <sqlite3.h>

#include "sqlnx/sqlite/native/flags.hpp"

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        // Shared by binding (statement) and SQL function results (context)

        inline sqlite3_destructor_type destructor_for(bind_mode mode) noexcept
        {
            if (mode == bind_mode::no_copy)
            {
                return SQLITE_STATIC;
            }
            return SQLITE_TRANSIENT;
        }

        // SQLite binds NULL for a null pointer, but an empty string_view may well have one. Text must stay text.
        inline const char* non_null(const char* text) noexcept
        {
            if (text == nullptr)
            {
                return "";
            }
            return text;
        }

        inline const char16_t* non_null(const char16_t* text) noexcept
        {
            if (text == nullptr)
            {
                return u"";
            }
            return text;
        }
    } // native
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_DETAIL_TEXT_AND_BLOB_HPP
