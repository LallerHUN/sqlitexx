#ifndef SQLITEXX_VALUE_HPP
#define SQLITEXX_VALUE_HPP

// How C++ types become statement parameters and SQL function results, and how column values and SQL function
// arguments become C++ types.
//
// Built in: integers, bool, floating point, std::string, string_view, cstring_view, const char* (binding only), nullptr
// (binding only), blob, zeroblob (binding only, sqlitexx/zeroblob.hpp), and optional<T> of any of these for nullable
// values. From C++17 and C++20 on also std::byte and std::span blobs (sqlitexx/value_bytes.hpp) and char8_t text
// (sqlitexx/value_char8.hpp). Other types plug in by specializing value_traits:
//
//     template <>
//     struct sqlitexx::value_traits<my_type>
//     {
//         static native::result bind(native::statement& stmt, int index, const my_type& value);
//         static expected<my_type> read(const native::statement& stmt, int index);
//     };
//
// read() is only called for non-NULL values; reading NULL into a type other than optional<T> is an error
// (primary_code::mismatch) before read() is reached.
//
// Traits written for native::statement, as above, work for statements. To work for SQL functions too (arguments and
// results, see sqlitexx/function.hpp), make bind() and read() templates over the statement type, as the built-in
// traits are: SQL functions pass objects with the same bind_* and column_* members.
//
//     template <class Target>
//     static native::result bind(Target& stmt, int index, const my_type& value);
//     template <class Source>
//     static expected<my_type> read(const Source& stmt, int index);

#include "sqlitexx/config.hpp"
#include "sqlitexx/cstring_view.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/statement.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace sqlitexx
{
using blob = std::vector<unsigned char>;

template <class T, class Enable = void>
struct value_traits;

namespace detail
{
    template <class T>
    struct is_optional : std::false_type
    {
    };

    template <class T>
    struct is_optional<optional<T>> : std::true_type
    {
    };

    inline native::result mismatch_result() noexcept
    {
        return native::result(static_cast<int>(native::primary_code::mismatch));
    }

    // What error messages call the value at `index`: "column 2" here, "argument 1" for SQL function arguments
    inline std::string value_name(
        const native::statement&,
        int index
    )
    {
        return "column " + std::to_string(index);
    }

    // Finds value_name for any source: the overloads above, and through argument-dependent lookup the ones declared
    // next to other sources
    template <class Source>
    std::string name_of(
        const Source& source,
        int index
    )
    {
        return value_name(source, index);
    }

    // The text of a non-NULL value. SQLite gives none only when converting the value to text ran out of memory.
    template <class Source>
    expected<cstring_view> read_text(
        const Source& stmt,
        int index
    )
    {
        const optional<cstring_view> text = stmt.column_text(index);
        if (!text)
        {
            return expected<cstring_view>(
                unexpect,
                error(
                    native::result(static_cast<int>(native::primary_code::nomem)),
                    name_of(stmt, index) + ": out of memory converting the value to text"
                )
            );
        }
        return *text;
    }

    inline error out_of_range(const std::string& name)
    {
        return error(mismatch_result(), name + ": the integer does not fit the requested type");
    }

    // Integer range checks without comparisons a compiler can call always true (-Wtype-limits): each kind of
    // integer gets the one check it needs.
    using signed_64_bit = std::integral_constant<int, 0>;
    using signed_narrower = std::integral_constant<int, 1>;
    using unsigned_64_bit = std::integral_constant<int, 2>;
    using unsigned_narrower = std::integral_constant<int, 3>;

    template <class T>
    using integer_kind =
        std::integral_constant<int, (std::is_signed<T>::value ? 0 : 2) + (sizeof(T) >= sizeof(std::int64_t) ? 0 : 1)>;

    template <class T>
    bool fits_int64(
        T,
        signed_64_bit
    ) noexcept
    {
        return true;
    }

    template <class T>
    bool fits_int64(
        T,
        signed_narrower
    ) noexcept
    {
        return true;
    }

    template <class T>
    bool fits_int64(
        T value,
        unsigned_64_bit
    ) noexcept
    {
        return value <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
    }

    template <class T>
    bool fits_int64(
        T,
        unsigned_narrower
    ) noexcept
    {
        return true;
    }

    template <class T>
    bool fits_type(
        std::int64_t,
        signed_64_bit
    ) noexcept
    {
        return true;
    }

    template <class T>
    bool fits_type(
        std::int64_t value,
        signed_narrower
    ) noexcept
    {
        return value >= static_cast<std::int64_t>((std::numeric_limits<T>::min)())
            && value <= static_cast<std::int64_t>((std::numeric_limits<T>::max)());
    }

    template <class T>
    bool fits_type(
        std::int64_t value,
        unsigned_64_bit
    ) noexcept
    {
        return value >= 0;
    }

    template <class T>
    bool fits_type(
        std::int64_t value,
        unsigned_narrower
    ) noexcept
    {
        return value >= 0 && value <= static_cast<std::int64_t>((std::numeric_limits<T>::max)());
    }
} // detail

// Integers other than bool. They are stored as 64-bit signed integers; values that do not fit (in either
// direction) are a primary_code::mismatch error, never silently wrapped.
template <class T>
struct value_traits<T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value>::type>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        T value
    ) noexcept
    {
        if (!detail::fits_int64(value, detail::integer_kind<T>()))
        {
            return detail::mismatch_result();
        }
        return stmt.bind_int64(index, static_cast<std::int64_t>(value));
    }

    template <class Source>
    static expected<T> read(
        const Source& stmt,
        int index
    )
    {
        const std::int64_t value = stmt.column_int64(index);
        if (!detail::fits_type<T>(value, detail::integer_kind<T>()))
        {
            return expected<T>(unexpect, detail::out_of_range(detail::name_of(stmt, index)));
        }
        return static_cast<T>(value);
    }
};

template <>
struct value_traits<bool>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        bool value
    ) noexcept
    {
        return stmt.bind_int(index, value ? 1 : 0);
    }

    template <class Source>
    static expected<bool> read(
        const Source& stmt,
        int index
    )
    {
        return stmt.column_int64(index) != 0;
    }
};

template <class T>
struct value_traits<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        T value
    ) noexcept
    {
        return stmt.bind_double(index, static_cast<double>(value));
    }

    template <class Source>
    static expected<T> read(
        const Source& stmt,
        int index
    )
    {
        return static_cast<T>(stmt.column_double(index));
    }
};

template <>
struct value_traits<std::string>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const std::string& value
    ) noexcept
    {
        return stmt.bind_text(index, string_view(value));
    }

    template <class Source>
    static expected<std::string> read(
        const Source& stmt,
        int index
    )
    {
        const expected<cstring_view> text = detail::read_text(stmt, index);
        if (!text)
        {
            return expected<std::string>(unexpect, text.error());
        }
        return std::string(text->data(), text->size());
    }
};

// Reading a string_view gives a view into SQLite's memory: valid until the next step(), reset() or finalize().
template <>
struct value_traits<string_view>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        string_view value
    ) noexcept
    {
        return stmt.bind_text(index, value);
    }

    template <class Source>
    static expected<string_view> read(
        const Source& stmt,
        int index
    )
    {
        const expected<cstring_view> text = detail::read_text(stmt, index);
        if (!text)
        {
            return expected<string_view>(unexpect, text.error());
        }
        return text->view();
    }
};

// Like string_view, and always followed by a '\0', so c_str() can go to C functions.
template <>
struct value_traits<cstring_view>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        cstring_view value
    ) noexcept
    {
        return stmt.bind_text(index, value.view());
    }

    template <class Source>
    static expected<cstring_view> read(
        const Source& stmt,
        int index
    )
    {
        return detail::read_text(stmt, index);
    }
};

// Binding only: reading into a const char* would point into memory that the next step() reuses.
template <>
struct value_traits<const char*>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const char* value
    ) noexcept
    {
        if (value == nullptr)
        {
            return stmt.bind_null(index);
        }
        return stmt.bind_text(index, string_view(value));
    }
};

template <>
struct value_traits<char*> : value_traits<const char*>
{
};

// Binding only: binds NULL.
template <>
struct value_traits<std::nullptr_t>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        std::nullptr_t
    ) noexcept
    {
        return stmt.bind_null(index);
    }
};

template <>
struct value_traits<blob>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const blob& value
    ) noexcept
    {
        return stmt.bind_blob(index, value.data(), value.size());
    }

    template <class Source>
    static expected<blob> read(
        const Source& stmt,
        int index
    )
    {
        const unsigned char* data = static_cast<const unsigned char*>(stmt.column_blob(index));
        const std::size_t size = stmt.column_bytes(index);
        if (data == nullptr)
        {
            return blob();
        }
        return blob(data, data + size);
    }
};

// NULL when empty, the value otherwise.
template <class T>
struct value_traits<optional<T>>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const optional<T>& value
    )
    {
        if (!value)
        {
            return stmt.bind_null(index);
        }
        return value_traits<T>::bind(stmt, index, *value);
    }

    template <class Source>
    static expected<optional<T>> read(
        const Source& stmt,
        int index
    )
    {
        if (stmt.column_type(index) == native::value_type::null)
        {
            return optional<T>();
        }
        expected<T> value = value_traits<T>::read(stmt, index);
        if (!value)
        {
            return expected<optional<T>>(unexpect, value.error());
        }
        return optional<T>(std::move(*value));
    }
};

} // sqlitexx

// Types from newer standards; each header declares nothing before its standard. They come last because they build on
// the definitions above.
#include "sqlitexx/value_bytes.hpp"
#include "sqlitexx/value_char8.hpp"
#include "sqlitexx/zeroblob.hpp"

#endif // SQLITEXX_VALUE_HPP
