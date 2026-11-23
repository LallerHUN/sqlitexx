#ifndef SQLITEXX_VALUE_CHAR8_HPP
#define SQLITEXX_VALUE_CHAR8_HPP

// UTF-8 text as char8_t (C++20): u8"..." literals, std::u8string, std::u8string_view and u8cstring_view bind and read
// as text, like their char counterparts. Before C++20 this header declares nothing, so it is safe to include in every
// standard; sqlitexx/value.hpp includes it.

#include "sqlitexx/value.hpp"

#if defined(__cpp_char8_t) && defined(__cpp_lib_char8_t)

#include <cstring>
#include <string>
#include <string_view>

namespace sqlitexx
{
namespace detail
{
    // SQLite stores UTF-8 as char; char may view any object, so char8_t text can go in as char.
    inline string_view as_char_text(std::u8string_view text) noexcept
    {
        return string_view(reinterpret_cast<const char*>(text.data()), text.size());
    }

    // SQLite's text viewed as char8_t, the way UTF-8 libraries view char buffers. Owning copies are made with memcpy
    // instead (see value_traits<std::u8string>).
    inline const char8_t* as_char8_text(const char* text) noexcept
    {
        return reinterpret_cast<const char8_t*>(text);
    }
} // detail

// Binding only, like const char*: binds NULL for a null pointer.
template <>
struct value_traits<const char8_t*>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const char8_t* value
    ) noexcept
    {
        if (value == nullptr)
        {
            return stmt.bind_null(index);
        }
        return stmt.bind_text(index, detail::as_char_text(std::u8string_view(value)));
    }
};

template <>
struct value_traits<char8_t*> : value_traits<const char8_t*>
{
};

template <>
struct value_traits<std::u8string>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const std::u8string& value
    ) noexcept
    {
        return stmt.bind_text(index, detail::as_char_text(value));
    }

    template <class Source>
    static expected<std::u8string> read(
        const Source& stmt,
        int index
    )
    {
        const expected<cstring_view> text = detail::read_text(stmt, index);
        if (!text)
        {
            return expected<std::u8string>(unexpect, text.error());
        }
        std::u8string copy(text->size(), u8'\0');
        if (!copy.empty())
        {
            std::memcpy(copy.data(), text->data(), text->size());
        }
        return copy;
    }
};

// Reading gives a view into SQLite's memory: valid until the next step(), reset() or finalize().
template <>
struct value_traits<std::u8string_view>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        std::u8string_view value
    ) noexcept
    {
        return stmt.bind_text(index, detail::as_char_text(value));
    }

    template <class Source>
    static expected<std::u8string_view> read(
        const Source& stmt,
        int index
    )
    {
        const expected<cstring_view> text = detail::read_text(stmt, index);
        if (!text)
        {
            return expected<std::u8string_view>(unexpect, text.error());
        }
        return std::u8string_view(detail::as_char8_text(text->data()), text->size());
    }
};

// Like std::u8string_view, and always followed by a '\0', so c_str() can go to C functions.
template <>
struct value_traits<u8cstring_view>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        u8cstring_view value
    ) noexcept
    {
        return stmt.bind_text(index, detail::as_char_text(std::u8string_view(value.data(), value.size())));
    }

    template <class Source>
    static expected<u8cstring_view> read(
        const Source& stmt,
        int index
    )
    {
        const expected<cstring_view> text = detail::read_text(stmt, index);
        if (!text)
        {
            return expected<u8cstring_view>(unexpect, text.error());
        }
        return u8cstring_view::assume_terminated(detail::as_char8_text(text->c_str()), text->size());
    }
};
} // sqlitexx

#endif

#endif // SQLITEXX_VALUE_CHAR8_HPP
