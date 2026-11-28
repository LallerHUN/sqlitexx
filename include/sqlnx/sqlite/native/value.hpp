#ifndef SQLNX_SQLITE_NATIVE_VALUE_HPP
#define SQLNX_SQLITE_NATIVE_VALUE_HPP

#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/cstring_view.hpp"
#include "sqlnx/sqlite/native/flags.hpp"

#include <cstddef>
#include <cstdint>

extern "C"
{
    struct sqlite3_value;
}

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        /// One argument of an SQL function (sqlite3_value), as SQLite passes it to the function. A view: it does not
        /// own the value, which is valid only during the call. Mirrors sqlite3_value_* one to one and never throws.
        class value final
        {
        public:
            explicit value(sqlite3_value* handle) noexcept;

            sqlite3_value* handle() const noexcept;

            value_type type() const noexcept;
            int to_int() const noexcept;
            std::int64_t to_int64() const noexcept;
            double to_double() const noexcept;

            /// As statement::column_text: always followed by a '\0', nullopt for NULL or when converting the value to
            /// text ran out of memory. Valid until the function returns, or until the value is read in another
            /// encoding.
            optional<cstring_view> text() const noexcept;
            optional<u16cstring_view> text16() const noexcept;

            /// As statement::column_blob: NULL for a NULL value and for a zero-length blob alike.
            const void* blob() const noexcept;
            std::size_t bytes() const noexcept;

        private:
            sqlite3_value* val;
        };

    } // native
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_NATIVE_VALUE_HPP
