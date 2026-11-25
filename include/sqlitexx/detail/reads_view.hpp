#ifndef SQLITEXX_DETAIL_READS_VIEW_HPP
#define SQLITEXX_DETAIL_READS_VIEW_HPP

/// \file
/// Whether reading a type gives a view into SQLite's memory rather than a value of its own. Such a view ends with the
/// statement's next step, reset or finalize, so calls that finalize the statement before returning
/// (database::query_row) reject these types at compile time.

#include "sqlitexx/detail/noexcept_traits.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/value.hpp"

#include <type_traits>

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
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_READS_VIEW_HPP
