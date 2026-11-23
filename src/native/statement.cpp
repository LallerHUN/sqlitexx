#include "impl_sqlitexx.hpp"

namespace sqlitexx
{
namespace native
{
    namespace
    {
        sqlite3_destructor_type destructor_for(bind_mode mode) noexcept
        {
            if (mode == bind_mode::no_copy)
            {
                return SQLITE_STATIC;
            }
            return SQLITE_TRANSIENT;
        }

        // SQLite binds NULL for a null pointer, but an empty string_view may well have one. Text must stay text.
        const char* non_null(const char* text) noexcept
        {
            if (text == nullptr)
            {
                return "";
            }
            return text;
        }

        const char16_t* non_null(const char16_t* text) noexcept
        {
            if (text == nullptr)
            {
                return u"";
            }
            return text;
        }
    } // anonymous

    statement::statement(sqlite3_stmt* handle) noexcept
        : stmt(handle)
    {
    }

    statement::statement(statement&& other) noexcept
        : stmt(other.release())
    {
    }

    statement& statement::operator=(statement&& other) noexcept
    {
        if (this != &other)
        {
            finalize();
            stmt = other.release();
        }
        return *this;
    }

    statement::~statement()
    {
        finalize();
    }

    result statement::finalize() noexcept
    {
        // sqlite3_finalize returns the error of the most recent step, which the caller has already seen; finalizing
        // itself always succeeds.
        const int rc = sqlite3_finalize(stmt);
        stmt = nullptr;
        return result(rc);
    }

    sqlite3_stmt* statement::handle() const noexcept
    {
        return stmt;
    }

    sqlite3_stmt* statement::release() noexcept
    {
        sqlite3_stmt* handle = stmt;
        stmt = nullptr;
        return handle;
    }

    statement::operator bool() const noexcept
    {
        return stmt != nullptr;
    }

    result statement::step() noexcept
    {
        return result(sqlite3_step(stmt));
    }

    result statement::reset() noexcept
    {
        return result(sqlite3_reset(stmt));
    }

    result statement::clear_bindings() noexcept
    {
        return result(sqlite3_clear_bindings(stmt));
    }

    bool statement::readonly() const noexcept
    {
        return sqlite3_stmt_readonly(stmt) != 0;
    }

    bool statement::busy() const noexcept
    {
        return sqlite3_stmt_busy(stmt) != 0;
    }

    const char* statement::sql() const noexcept
    {
        return sqlite3_sql(stmt);
    }

    result statement::bind_null(int index) noexcept
    {
        return result(sqlite3_bind_null(stmt, index));
    }

    result statement::bind_int(
        int index,
        int value
    ) noexcept
    {
        return result(sqlite3_bind_int(stmt, index, value));
    }

    result statement::bind_int64(
        int index,
        std::int64_t value
    ) noexcept
    {
        return result(sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(value)));
    }

    result statement::bind_double(
        int index,
        double value
    ) noexcept
    {
        return result(sqlite3_bind_double(stmt, index, value));
    }

    result statement::bind_text(
        int index,
        string_view value,
        bind_mode mode
    ) noexcept
    {
        return result(sqlite3_bind_text64(
            stmt,
            index,
            non_null(value.data()),
            static_cast<sqlite3_uint64>(value.size()),
            destructor_for(mode),
            SQLITE_UTF8
        ));
    }

    result statement::bind_text16(
        int index,
        u16string_view value,
        bind_mode mode
    ) noexcept
    {
        return result(sqlite3_bind_text64(
            stmt,
            index,
            reinterpret_cast<const char*>(non_null(value.data())),
            static_cast<sqlite3_uint64>(value.size() * sizeof(char16_t)),
            destructor_for(mode),
            SQLITE_UTF16 // native byte order
        ));
    }

    result statement::bind_blob(
        int index,
        const void* data,
        std::size_t size,
        bind_mode mode
    ) noexcept
    {
        if (data == nullptr)
        {
            // An empty blob, not NULL (same reasoning as for text)
            return bind_zeroblob(index, 0);
        }
        return result(sqlite3_bind_blob64(stmt, index, data, static_cast<sqlite3_uint64>(size), destructor_for(mode)));
    }

    result statement::bind_zeroblob(
        int index,
        std::uint64_t size
    ) noexcept
    {
        return result(sqlite3_bind_zeroblob64(stmt, index, static_cast<sqlite3_uint64>(size)));
    }

    int statement::bind_parameter_count() const noexcept
    {
        return sqlite3_bind_parameter_count(stmt);
    }

    int statement::bind_parameter_index(const char* name) const noexcept
    {
        return sqlite3_bind_parameter_index(stmt, name);
    }

    const char* statement::bind_parameter_name(int index) const noexcept
    {
        return sqlite3_bind_parameter_name(stmt, index);
    }

    int statement::column_count() const noexcept
    {
        return sqlite3_column_count(stmt);
    }

    int statement::data_count() const noexcept
    {
        return sqlite3_data_count(stmt);
    }

    const char* statement::column_name(int index) const noexcept
    {
        return sqlite3_column_name(stmt, index);
    }

    const char* statement::column_decltype(int index) const noexcept
    {
        return sqlite3_column_decltype(stmt, index);
    }

    value_type statement::column_type(int index) const noexcept
    {
        return static_cast<value_type>(sqlite3_column_type(stmt, index));
    }

    int statement::column_int(int index) const noexcept
    {
        return sqlite3_column_int(stmt, index);
    }

    std::int64_t statement::column_int64(int index) const noexcept
    {
        return static_cast<std::int64_t>(sqlite3_column_int64(stmt, index));
    }

    double statement::column_double(int index) const noexcept
    {
        return sqlite3_column_double(stmt, index);
    }

    string_view statement::column_text(int index) const noexcept
    {
        // Pointer first, then the size: reading the text may convert it, which changes the size.
        const unsigned char* text = sqlite3_column_text(stmt, index);
        if (text == nullptr)
        {
            return string_view();
        }
        const int bytes = sqlite3_column_bytes(stmt, index);
        return string_view(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
    }

    u16string_view statement::column_text16(int index) const noexcept
    {
        const void* text = sqlite3_column_text16(stmt, index);
        if (text == nullptr)
        {
            return u16string_view();
        }
        const int bytes = sqlite3_column_bytes16(stmt, index);
        return u16string_view(static_cast<const char16_t*>(text), static_cast<std::size_t>(bytes) / sizeof(char16_t));
    }

    const void* statement::column_blob(int index) const noexcept
    {
        return sqlite3_column_blob(stmt, index);
    }

    std::size_t statement::column_bytes(int index) const noexcept
    {
        return static_cast<std::size_t>(sqlite3_column_bytes(stmt, index));
    }

} // native
} // sqlitexx
