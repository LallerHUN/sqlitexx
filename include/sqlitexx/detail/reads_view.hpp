#ifndef SQLITEXX_DETAIL_READS_VIEW_HPP
#define SQLITEXX_DETAIL_READS_VIEW_HPP

/// \file
/// Whether reading a type gives a view into SQLite's memory rather than a value of its own. Such a view ends with the
/// statement's next step, reset or finalize, so calls that end the statement before the row is returned
/// (results::first() on a temporary, the first<T...>() chaining step on a temporary statement) reject these types at
/// compile time.

#include "sqlitexx/detail/noexcept_traits.hpp"
#include "sqlitexx/detail/row_fields.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/value.hpp"

#include <tuple>
#include <type_traits>

/// The static_assert message for reading a view out of a statement that is about to end
#define SQLITEXX_DETAIL_VIEW_WOULD_DANGLE \
    "the statement ends before the row is returned, so a view into it would dangle: read std::string or a blob " \
    "instead of string_view, cstring_view or std::span, or keep the statement (or results) in a variable"

namespace sqlitexx
{
namespace detail
{
    /// True when value_traits<T> declares `static constexpr bool reads_view = true;`
    template <class T, class Enable = void>
    struct reads_view : std::false_type
    {
    };

    template <class T>
    struct reads_view<T, typename make_void<decltype(value_traits<T>::reads_view)>::type>
        : std::integral_constant<bool, value_traits<T>::reads_view>
    {
    };

    /// An optional view is still a view
    template <class T>
    struct reads_view<optional<T>> : reads_view<T>
    {
    };

    template <class... T>
    struct any_reads_view
        : std::integral_constant<bool, !all_of<std::integral_constant<bool, !reads_view<T>::value>...>::value>
    {
    };

    /// A struct with fields is a view when one of its members is
    template <class Members>
    struct any_member_reads_view;

    template <class... Member>
    struct any_member_reads_view<std::tuple<Member...>> : any_reads_view<Member...>
    {
    };

    template <class T>
    struct reads_view<T, typename std::enable_if<has_fields<T>::value>::type>
        : any_member_reads_view<typename field_member_types<typename field_list<T>::type>::type>
    {
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_READS_VIEW_HPP
