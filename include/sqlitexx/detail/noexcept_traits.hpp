#ifndef SQLITEXX_DETAIL_NOEXCEPT_TRAITS_HPP
#define SQLITEXX_DETAIL_NOEXCEPT_TRAITS_HPP

// Whether binding and reading types cannot throw, for the noexcept of the statement calls that bind and read them:
// they are noexcept exactly when the value_traits they use are.

#include "sqlitexx/native/statement.hpp"
#include "sqlitexx/value.hpp"

#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    // std::conjunction, which arrives only with C++17
    template <class... Conditions>
    struct all_of : std::true_type
    {
    };

    template <class First, class... Rest>
    struct all_of<First, Rest...> : std::integral_constant<bool, First::value && all_of<Rest...>::value>
    {
    };

    // Binding a T to a statement parameter
    template <class T>
    struct binds_noexcept : std::integral_constant<
                                bool,
                                noexcept(value_traits<typename std::decay<T>::type>::
                                             bind(std::declval<native::statement&>(), 0, std::declval<const T&>()))>
    {
    };

    template <class... T>
    struct all_bind_noexcept : all_of<binds_noexcept<T>...>
    {
    };

    // Reading a column as T, and moving the value on
    template <class T>
    struct reads_noexcept : std::integral_constant<
                                bool,
                                noexcept(value_traits<T>::read(std::declval<const native::statement&>(), 0))
                                    && std::is_nothrow_move_constructible<T>::value>
    {
    };

    // Reading a row: a row of several columns is a tuple, made empty and then filled
    template <class... T>
    struct row_reads_noexcept : std::integral_constant<
                                    bool,
                                    all_of<reads_noexcept<T>...>::value
                                        && (sizeof...(T) == 1
                                            || (std::is_nothrow_default_constructible<std::tuple<T...>>::value
                                                && all_of<std::is_nothrow_move_assignable<T>...>::value))>
    {
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_NOEXCEPT_TRAITS_HPP
