#ifndef SQLITEXX_DETAIL_FIELD_RECORD_HPP
#define SQLITEXX_DETAIL_FIELD_RECORD_HPP

/// \file
/// The record_traits of a struct whose fields SQLITEXX_FIELDS names: each field from the column with its name.

#include "sqlitexx/detail/noexcept_traits.hpp"
#include "sqlitexx/detail/row_fields.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/record_traits.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
template <class T>
struct record_traits<T, typename std::enable_if<detail::has_fields<T>::value>::type>
{
    /// Makes a T and fills each field from the column with its name. A field without a column is the
    /// primary_code::range error that column_index() gives; columns without a field are ignored.
    template <class Source>
    static expected<T> read(const Source& row) noexcept(
        detail::fields_read_noexcept<
            T,
            typename detail::field_member_types<typename detail::field_list<T>::type>::type>::value
    )
    {
        static_assert(
            std::is_default_constructible<T>::value,
            "a struct with fields must be default-constructible: sqlitexx makes it, then fills its fields; specialize "
            "record_traits (with record_builder) for a type that needs constructor arguments"
        );
        using list_type = typename detail::field_list<T>::type;
        const list_type list = sqlitexx_fields(static_cast<const T*>(nullptr));
        T value{};
        expected<void> filled =
            fill<0>(row, value, list, std::integral_constant<bool, std::tuple_size<list_type>::value == 0>());
        if (!filled)
        {
            return expected<T>(unexpect, std::move(filled.error()));
        }
        return expected<T>(std::move(value));
    }

private:
    template <
        std::size_t I,
        class Source,
        class List>
    static expected<void> fill(
        const Source&,
        T&,
        const List&,
        std::true_type /*past the end*/
    ) noexcept
    {
        return {};
    }

    template <
        std::size_t I,
        class Source,
        class List>
    static expected<void> fill(
        const Source& row,
        T& value,
        const List& list,
        std::false_type /*past the end*/
    ) noexcept(detail::
                   fields_read_noexcept<
                       T,
                       typename detail::field_member_types<List>::type>::value)
    {
        using field_type = typename std::tuple_element<I, List>::type;
        using member = typename detail::field_member<field_type>::type;
        const field_type& field = std::get<I>(list);
        expected<int> index = row.column_index(field.name);
        if (!index)
        {
            return expected<void>(unexpect, std::move(index.error()));
        }
        expected<member> read = row.template column<member>(*index);
        if (!read)
        {
            return expected<void>(unexpect, std::move(read.error()));
        }
        value.*(field.member) = std::move(*read);
        return fill<I + 1>(row, value, list, std::integral_constant<bool, I + 1 == std::tuple_size<List>::value>());
    }
};
} // sqlitexx

#endif // SQLITEXX_DETAIL_FIELD_RECORD_HPP
