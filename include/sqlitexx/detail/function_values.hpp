#ifndef SQLITEXX_DETAIL_FUNCTION_VALUES_HPP
#define SQLITEXX_DETAIL_FUNCTION_VALUES_HPP

// SQL function arguments and results seen through the members value_traits use on a native::statement, so every
// value_traits whose bind() and read() are templates works for SQL functions too.

#include "sqlitexx/config.hpp"
#include "sqlitexx/cstring_view.hpp"
#include "sqlitexx/native/context.hpp"
#include "sqlitexx/native/flags.hpp"
#include "sqlitexx/native/result.hpp"
#include "sqlitexx/native/value.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace sqlitexx
{
namespace detail
{
    // The arguments of one call, read like the columns of a row (index counts from 0)
    class argument_source
    {
    public:
        explicit argument_source(sqlite3_value** values) noexcept
            : arguments(values)
        {
        }

        native::value_type column_type(int index) const noexcept
        {
            return at(index).type();
        }

        int column_int(int index) const noexcept
        {
            return at(index).to_int();
        }

        std::int64_t column_int64(int index) const noexcept
        {
            return at(index).to_int64();
        }

        double column_double(int index) const noexcept
        {
            return at(index).to_double();
        }

        optional<cstring_view> column_text(int index) const noexcept
        {
            return at(index).text();
        }

        optional<u16cstring_view> column_text16(int index) const noexcept
        {
            return at(index).text16();
        }

        const void* column_blob(int index) const noexcept
        {
            return at(index).blob();
        }

        std::size_t column_bytes(int index) const noexcept
        {
            return at(index).bytes();
        }

    private:
        sqlite3_value** arguments;

        native::value at(int index) const noexcept
        {
            return native::value(arguments[index]);
        }
    };

    // Error messages count arguments from 1, as SQL does
    inline std::string value_name(
        const argument_source&,
        int index
    )
    {
        return "argument " + std::to_string(index + 1);
    }

    // The result of one call, written like a statement parameter. The index is ignored, and text and blobs are always
    // copied: what the function returned is gone once it has returned.
    class result_target
    {
    public:
        explicit result_target(native::context& destination) noexcept
            : ctx(destination)
        {
        }

        native::result bind_null(int) noexcept
        {
            ctx.result_null();
            return native::result();
        }

        native::result bind_int(
            int,
            int value
        ) noexcept
        {
            ctx.result_int(value);
            return native::result();
        }

        native::result bind_int64(
            int,
            std::int64_t value
        ) noexcept
        {
            ctx.result_int64(value);
            return native::result();
        }

        native::result bind_double(
            int,
            double value
        ) noexcept
        {
            ctx.result_double(value);
            return native::result();
        }

        native::result bind_text(
            int,
            string_view value,
            native::bind_mode = native::bind_mode::copy
        ) noexcept
        {
            ctx.result_text(value, native::bind_mode::copy);
            return native::result();
        }

        native::result bind_text16(
            int,
            u16string_view value,
            native::bind_mode = native::bind_mode::copy
        ) noexcept
        {
            ctx.result_text16(value, native::bind_mode::copy);
            return native::result();
        }

        native::result bind_blob(
            int,
            const void* data,
            std::size_t size,
            native::bind_mode = native::bind_mode::copy
        ) noexcept
        {
            ctx.result_blob(data, size, native::bind_mode::copy);
            return native::result();
        }

        native::result bind_zeroblob(
            int,
            std::uint64_t size
        ) noexcept
        {
            return ctx.result_zeroblob(size);
        }

    private:
        native::context& ctx;
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_FUNCTION_VALUES_HPP
