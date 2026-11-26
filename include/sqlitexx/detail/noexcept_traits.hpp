#ifndef SQLITEXX_DETAIL_NOEXCEPT_TRAITS_HPP
#define SQLITEXX_DETAIL_NOEXCEPT_TRAITS_HPP

/// \file
/// Whether binding and reading types cannot throw, for the noexcept of the statement calls that bind and read them:
/// they are noexcept exactly when the value_traits they use are.

#include "sqlitexx/detail/row_fields.hpp"
#include "sqlitexx/native/statement.hpp"
#include "sqlitexx/record_traits.hpp"
#include "sqlitexx/value.hpp"

#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    /// std::conjunction, which arrives only with C++17
    template <class... Conditions>
    struct all_of : std::true_type
    {
    };

    template <class First, class... Rest>
    struct all_of<First, Rest...> : std::integral_constant<bool, First::value && all_of<Rest...>::value>
    {
    };

    /// Binding a T to a statement parameter, or a record to all of them
    template <class T, bool Record = is_record<typename std::decay<T>::type>::value>
    struct binds_noexcept : std::integral_constant<
                                bool,
                                noexcept(value_traits<typename std::decay<T>::type>::
                                             bind(std::declval<native::statement&>(), 0, std::declval<const T&>()))>
    {
    };

    /// A record binds through record_traits<T>::bind(); one without it doesn't bind at all (bind() says so)
    template <class T, bool Binds = is_bind_record<typename std::decay<T>::type>::value>
    struct record_binds_noexcept
        : std::integral_constant<
              bool,
              noexcept(record_traits<
                       typename std::decay<T>::type>::bind(std::declval<statement&>(), std::declval<const T&>()))>
    {
    };

    template <class T>
    struct record_binds_noexcept<T, false> : std::false_type
    {
    };

    template <class T>
    struct binds_noexcept<T, true> : record_binds_noexcept<T>
    {
    };

    template <class... T>
    struct all_bind_noexcept : all_of<binds_noexcept<T>...>
    {
    };

    /// Reading a column as T, and moving the value on. A record is read by its record_traits.
    template <class T, bool Record = is_record<T>::value>
    struct reads_noexcept : std::integral_constant<
                                bool,
                                noexcept(value_traits<T>::read(std::declval<const native::statement&>(), 0))
                                    && std::is_nothrow_move_constructible<T>::value>
    {
    };

    template <class T, class Members>
    struct fields_read_noexcept;

    template <class T, class... Member>
    struct fields_read_noexcept<T, std::tuple<Member...>>
        : std::integral_constant<
              bool,
              all_of<reads_noexcept<Member>...>::value && all_of<std::is_nothrow_move_assignable<Member>...>::value
                  && std::is_nothrow_default_constructible<T>::value && std::is_nothrow_move_constructible<T>::value>
    {
    };

    /// Binding each member of a struct with fields
    template <class Members>
    struct fields_bind_noexcept;

    template <class... Member>
    struct fields_bind_noexcept<std::tuple<Member...>> : all_of<binds_noexcept<Member>...>
    {
    };

    template <class T>
    struct reads_noexcept<T, true> : std::integral_constant<
                                         bool,
                                         noexcept(record_traits<T>::read(std::declval<const statement&>()))
                                             && std::is_nothrow_move_constructible<T>::value>
    {
    };

    /// Reading a row: a row of several columns is a tuple, made empty and then filled
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
