#ifndef SQLITEXX_DETAIL_ROW_FIELDS_HPP
#define SQLITEXX_DETAIL_ROW_FIELDS_HPP

/// \file
/// Whether a type is read as a struct of fields (sqlitexx/fields.hpp), and the types of its members.

#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/fields.hpp"
#include "sqlitexx/native/statement.hpp"
#include "sqlitexx/value.hpp"

#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    /// What sqlitexx_fields() gives for T, found by argument-dependent lookup
    template <class T>
    struct field_list
    {
        using type = decltype(sqlitexx_fields(static_cast<const T*>(nullptr)));
    };

    /// True when SQLITEXX_FIELDS (or a hand-written sqlitexx_fields) names T's fields
    template <class T, class Enable = void>
    struct has_fields : std::false_type
    {
    };

    template <class T>
    struct has_fields<T, typename make_void<decltype(sqlitexx_fields(static_cast<const T*>(nullptr)))>::type>
        : std::true_type
    {
    };

    template <class... T>
    struct any_has_fields : std::false_type
    {
    };

    template <class First, class... Rest>
    struct any_has_fields<First, Rest...>
        : std::integral_constant<bool, has_fields<First>::value || any_has_fields<Rest...>::value>
    {
    };

    /// The member type of one field (a base class's member keeps the base as its class)
    template <class Field>
    struct field_member;

    template <class Class, class Member>
    struct field_member<field_mapping<Class, Member>>
    {
        using type = Member;
    };

    /// The member types of a field list, as a std::tuple
    template <class List>
    struct field_member_types;

    template <class... Field>
    struct field_member_types<std::tuple<Field...>>
    {
        using type = std::tuple<typename field_member<Field>::type...>;
    };

    /// True when value_traits<T> can read T, for refusing a type that has both value_traits and fields
    template <class T, class Enable = void>
    struct has_read_traits : std::false_type
    {
    };

    template <class T>
    struct has_read_traits<
        T,
        typename make_void<decltype(value_traits<T>::read(std::declval<const native::statement&>(), 0))>::type>
        : std::true_type
    {
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_ROW_FIELDS_HPP
