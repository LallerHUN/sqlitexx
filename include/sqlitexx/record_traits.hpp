#ifndef SQLITEXX_RECORD_TRAITS_HPP
#define SQLITEXX_RECORD_TRAITS_HPP

/// \file
/// How a type fills a whole row, as value_traits says how a value fills one column.
///
/// A type is a record when record_traits<T> has a read() that takes the statement, positioned on a row:
///
///     template <>
///     struct sqlitexx::record_traits<point>
///     {
///         static sqlitexx::expected<point> read(const sqlitexx::statement& row)
///         {
///             return sqlitexx::record_builder(row).column<double>("x").column<double>("y").build<point>();
///         }
///     };
///
///     sqlitexx::expected<std::vector<point>> points = db.query<point>("SELECT x, y FROM shape").all();
///
/// A record is read wherever a row is read: query<point>, results::first, all and next, statement::rows<point> and
/// first<point>, for_each_row<point>. It is read on its own, not alongside other types, and must not also have
/// value_traits.
///
/// The structs that SQLITEXX_FIELDS names (sqlitexx/fields.hpp) are records already: the default record_traits fills
/// their fields by column name. Specialize record_traits for what a field list can't do: a type without a default
/// constructor, computed or checked fields, or a whole class template through a partial specialization.
/// sqlitexx/record_builder.hpp reads the columns without an error check after each.
///
/// Binding goes the other way: record_traits<T>::bind(statement&, const T&) binds a T to the statement's
/// parameters, so a record is an argument wherever values are bound, on its own:
///
///     db.execute("INSERT INTO shape(x, y) VALUES(:x, :y)", p);
///
/// sqlitexx/record_binder.hpp binds the values without an error check after each.
///
/// A record whose read() keeps views into the row (string_view, cstring_view, std::span) declares
/// `static constexpr bool reads_view = true;`, so the calls that would let such a view dangle refuse it at compile
/// time, as for value_traits. Records from SQLITEXX_FIELDS get this from their members.

#include "sqlitexx/detail/utility.hpp"

#include <type_traits>
#include <utility>

namespace sqlitexx
{
class statement;

/// Not a record: the primary template has no read(). Specialize it, or name the fields with SQLITEXX_FIELDS.
template <class T, class Enable = void>
struct record_traits
{
};

namespace detail
{
    /// True when record_traits<T> can read T from a row
    template <class T, class Enable = void>
    struct is_record : std::false_type
    {
    };

    template <class T>
    struct is_record<T, typename make_void<decltype(record_traits<T>::read(std::declval<const statement&>()))>::type>
        : std::true_type
    {
    };

    /// True when record_traits<T> can bind a T to a statement's parameters
    template <class T, class Enable = void>
    struct is_bind_record : std::false_type
    {
    };

    template <class T>
    struct is_bind_record<
        T,
        typename make_void<
            decltype(record_traits<T>::bind(std::declval<statement&>(), std::declval<const T&>()))>::type>
        : std::true_type
    {
    };

    template <class... T>
    struct any_is_record : std::false_type
    {
    };

    template <class First, class... Rest>
    struct any_is_record<First, Rest...>
        : std::integral_constant<bool, is_record<First>::value || any_is_record<Rest...>::value>
    {
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_RECORD_TRAITS_HPP
