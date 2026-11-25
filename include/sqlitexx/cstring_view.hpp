#ifndef SQLITEXX_CSTRING_VIEW_HPP
#define SQLITEXX_CSTRING_VIEW_HPP

#include "sqlitexx/config.hpp"

#include <cstddef>
#include <string>

namespace sqlitexx
{
/// A read-only view of text that is followed by a '\0': data()[size()] is always '\0', so c_str() can go straight to C
/// functions. It never holds a null pointer; a default-constructed one views "".
///
/// Only sources that guarantee the terminator create one (C strings such as string literals, std::string, SQLite's
/// column text), and only remove_prefix() changes one, since it keeps the end of the text. Anything that cuts off the
/// end is a string_view: view() gives one, and a cstring_view converts to it implicitly.
///
/// Text may contain '\0' characters of its own. size() counts them, but a C function reading c_str() stops at the
/// first one, so it sees less of the text, never more: use data() and size() for text that may hold them.
template <class CharT>
class basic_cstring_view
{
public:
    using traits_type = std::char_traits<CharT>;
    using value_type = CharT;
    using size_type = std::size_t;
    using const_pointer = const CharT*;
    using const_reference = const CharT&;
    using const_iterator = const CharT*;
    using iterator = const_iterator;

    // Lifetime

    basic_cstring_view() noexcept
        : text(empty_text()),
          count(0)
    {
    }

    /// A null pointer views "", like a default-constructed view.
    basic_cstring_view(const CharT* source) noexcept
        : text(source == nullptr ? empty_text() : source),
          count(source == nullptr ? 0 : traits_type::length(source))
    {
    }

    template <class Allocator>
    basic_cstring_view(
        const std::basic_string<
            CharT,
            traits_type,
            Allocator>& source
    ) noexcept
        : text(source.c_str()),
          count(source.size())
    {
    }

    /// For callers who know that terminated[size] is '\0', with no check: text from a C API that guarantees it.
    static basic_cstring_view assume_terminated(
        const CharT* terminated,
        size_type size
    ) noexcept
    {
        return basic_cstring_view(terminated, size);
    }

    // Access

    const CharT* c_str() const noexcept
    {
        return text;
    }

    const CharT* data() const noexcept
    {
        return text;
    }

    size_type size() const noexcept
    {
        return count;
    }

    size_type length() const noexcept
    {
        return count;
    }

    bool empty() const noexcept
    {
        return count == 0;
    }

    const_iterator begin() const noexcept
    {
        return text;
    }

    const_iterator end() const noexcept
    {
        return text + count;
    }

    /// index <= size(); index == size() reads the terminator.
    const CharT& operator[](size_type index) const noexcept
    {
        return text[index];
    }

    /// Changing: drops the first n characters (n <= size()), which keeps the terminator.
    void remove_prefix(size_type n) noexcept
    {
        text += n;
        count -= n;
    }

    // Conversion

    basic_string_view<CharT> view() const noexcept
    {
        return basic_string_view<CharT>(text, count);
    }

    operator basic_string_view<CharT>() const noexcept
    {
        return view();
    }

    // Comparison, by content. Mixed comparisons go through string_view, which also takes C strings and std::string.
    // The template makes a comparison of two cstring_views prefer the exact match, and every other comparison prefer
    // the string_view overloads, whatever the standard.

    friend bool operator==(
        basic_cstring_view left,
        basic_string_view<CharT> right
    ) noexcept
    {
        return left.view() == right;
    }

    friend bool operator==(
        basic_string_view<CharT> left,
        basic_cstring_view right
    ) noexcept
    {
        return left == right.view();
    }

    template <class Unused = void>
    friend bool operator==(
        basic_cstring_view left,
        basic_cstring_view right
    ) noexcept
    {
        return left.view() == right.view();
    }

    friend bool operator!=(
        basic_cstring_view left,
        basic_string_view<CharT> right
    ) noexcept
    {
        return !(left == right);
    }

    friend bool operator!=(
        basic_string_view<CharT> left,
        basic_cstring_view right
    ) noexcept
    {
        return !(left == right);
    }

    template <class Unused = void>
    friend bool operator!=(
        basic_cstring_view left,
        basic_cstring_view right
    ) noexcept
    {
        return !(left.view() == right.view());
    }

private:
    const CharT* text;
    size_type count;

    basic_cstring_view(
        const CharT* terminated,
        size_type size
    ) noexcept
        : text(terminated),
          count(size)
    {
    }

    static const CharT* empty_text() noexcept
    {
        static const CharT terminator = CharT();
        return &terminator;
    }
};

using cstring_view = basic_cstring_view<char>;
using u16cstring_view = basic_cstring_view<char16_t>;

#if defined(__cpp_char8_t) && defined(__cpp_lib_char8_t)
/// UTF-8 text as char8_t (C++20)
using u8cstring_view = basic_cstring_view<char8_t>;
#endif
} // sqlitexx

#endif // SQLITEXX_CSTRING_VIEW_HPP
