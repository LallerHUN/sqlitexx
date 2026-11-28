#ifndef SQLNX_SQLITE_NATIVE_STATEMENT_HPP
#define SQLNX_SQLITE_NATIVE_STATEMENT_HPP

#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/cstring_view.hpp"
#include "sqlnx/sqlite/native/flags.hpp"
#include "sqlnx/sqlite/native/result.hpp"
#include "sqlnx/sqlite/native/statement_status.hpp"

#include <cstddef>
#include <cstdint>

extern "C"
{
    struct sqlite3_stmt;
}

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        /// A prepared statement (sqlite3_stmt). Move-only; the destructor finalizes it.
        ///
        /// Mirrors the C API one to one: functions return result codes and never throw. Bind and column indexes follow
        /// SQLite: parameters count from 1, columns from 0.
        class statement final
        {
        public:
            statement() noexcept = default;

            /// Takes ownership of a handle prepared elsewhere.
            explicit statement(sqlite3_stmt* handle) noexcept;

            statement(statement&& other) noexcept;
            statement& operator=(statement&& other) noexcept;

            statement(const statement&) = delete;
            statement& operator=(const statement&) = delete;

            ~statement();

            /// \name Lifetime
            ///@{
            result finalize() noexcept;
            sqlite3_stmt* handle() const noexcept;
            sqlite3_stmt* release() noexcept;
            explicit operator bool() const noexcept;
            ///@}

            /// \name Execution
            ///@{
            result step() noexcept;
            result reset() noexcept;
            result clear_bindings() noexcept;
            bool readonly() const noexcept;
            bool busy() const noexcept;
            const char* sql() const noexcept;
            /// sqlite3_stmt_status: the value of a counter, set back to zero afterwards when `reset_counter` is true; 0
            /// on an empty statement
            int status(
                statement_status counter,
                bool reset_counter = false
            ) noexcept;
            ///@}

            /// Binding (index counts from 1)
            result bind_null(int index) noexcept;
            result bind_int(
                int index,
                int value
            ) noexcept;
            result bind_int64(
                int index,
                std::int64_t value
            ) noexcept;
            result bind_double(
                int index,
                double value
            ) noexcept;
            result bind_text(
                int index,
                string_view value,
                bind_mode mode = bind_mode::copy
            ) noexcept;
            result bind_text16(
                int index,
                u16string_view value,
                bind_mode mode = bind_mode::copy
            ) noexcept;
            result bind_blob(
                int index,
                const void* data,
                std::size_t size,
                bind_mode mode = bind_mode::copy
            ) noexcept;
            result bind_zeroblob(
                int index,
                std::uint64_t size
            ) noexcept;
            int bind_parameter_count() const noexcept;

            /// 0 when there is no parameter with this name. The name includes its prefix: ":id", "@id" or "$id".
            int bind_parameter_index(const char* name) const noexcept;
            const char* bind_parameter_name(int index) const noexcept;

            /// Columns (index counts from 0)
            int column_count() const noexcept;
            int data_count() const noexcept;
            const char* column_name(int index) const noexcept;
            const char* column_decltype(int index) const noexcept;
            value_type column_type(int index) const noexcept;
            int column_int(int index) const noexcept;
            std::int64_t column_int64(int index) const noexcept;
            double column_double(int index) const noexcept;

            /// The text of a column, always followed by a '\0' (see cstring_view). nullopt when SQLite has no text to
            /// give, as sqlite3_column_text returns NULL: for a NULL value, or when converting the value to text ran
            /// out of memory (column_type() or connection::extended_errcode() tells which). An empty string is an empty
            /// view, never nullopt. The view stays valid until the next step(), reset() or finalize(), or until the
            /// same column is read in another encoding.
            optional<cstring_view> column_text(int index) const noexcept;
            optional<u16cstring_view> column_text16(int index) const noexcept;

            /// As in C: NULL for a NULL value and for a zero-length blob alike, so check column_type() or
            /// column_bytes().
            const void* column_blob(int index) const noexcept;
            std::size_t column_bytes(int index) const noexcept;

        private:
            sqlite3_stmt* stmt = nullptr;
        };

    } // native
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_NATIVE_STATEMENT_HPP
