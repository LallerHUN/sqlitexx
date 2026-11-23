#ifndef SQLITEXX_NATIVE_STATEMENT_HPP
#define SQLITEXX_NATIVE_STATEMENT_HPP

#include "sqlitexx/config.hpp"
#include "sqlitexx/native/flags.hpp"
#include "sqlitexx/native/result.hpp"

#include <cstddef>
#include <cstdint>

extern "C"
{
    struct sqlite3_stmt;
}

namespace sqlitexx
{
namespace native
{
    // A prepared statement (sqlite3_stmt). Move-only; the destructor finalizes it.
    //
    // Mirrors the C API one to one: functions return result codes and never throw. Bind and column indexes follow
    // SQLite: parameters count from 1, columns from 0.
    class statement final
    {
    public:
        statement() noexcept = default;

        // Takes ownership of a handle prepared elsewhere.
        explicit statement(sqlite3_stmt* handle) noexcept;

        statement(statement&& other) noexcept;
        statement& operator=(statement&& other) noexcept;

        statement(const statement&) = delete;
        statement& operator=(const statement&) = delete;

        ~statement();

        // Lifetime
        result finalize() noexcept;
        sqlite3_stmt* handle() const noexcept;
        sqlite3_stmt* release() noexcept;
        explicit operator bool() const noexcept;

        // Execution
        result step() noexcept;
        result reset() noexcept;
        result clear_bindings() noexcept;
        bool readonly() const noexcept;
        bool busy() const noexcept;
        const char* sql() const noexcept;

        // Binding (index counts from 1)
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

        // 0 when there is no parameter with this name. The name includes its prefix: ":id", "@id" or "$id".
        int bind_parameter_index(const char* name) const noexcept;
        const char* bind_parameter_name(int index) const noexcept;

        // Columns (index counts from 0)
        int column_count() const noexcept;
        int data_count() const noexcept;
        const char* column_name(int index) const noexcept;
        const char* column_decltype(int index) const noexcept;
        value_type column_type(int index) const noexcept;
        int column_int(int index) const noexcept;
        std::int64_t column_int64(int index) const noexcept;
        double column_double(int index) const noexcept;

        // The returned views stay valid until the next step(), reset() or finalize(), or until the same column is
        // read in another encoding. A NULL value reads as an empty view.
        string_view column_text(int index) const noexcept;
        u16string_view column_text16(int index) const noexcept;
        const void* column_blob(int index) const noexcept;
        std::size_t column_bytes(int index) const noexcept;

    private:
        sqlite3_stmt* stmt = nullptr;
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_STATEMENT_HPP
