#ifndef SQLNX_SQLITE_DETAIL_FIELD_RECORD_HPP
#define SQLNX_SQLITE_DETAIL_FIELD_RECORD_HPP

/// \file
/// The record_traits of a struct whose fields SQLNX_FIELDS names: each field from the column with its name.

#include "sqlnx/sqlite/detail/column_lookup.hpp"
#include "sqlnx/sqlite/detail/noexcept_traits.hpp"
#include "sqlnx/sqlite/detail/parameter_errors.hpp"
#include "sqlnx/sqlite/detail/row_fields.hpp"
#include "sqlnx/sqlite/expected.hpp"
#include "sqlnx/sqlite/record_traits.hpp"

#include <cstddef>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    template <class T>
    struct record_traits<T, typename std::enable_if<detail::has_fields<T>::value>::type>
    {
        /// Makes a T and fills each field from the column with its name. A field without a column is the
        /// primary_code::range error that column_index() gives; columns without a field are ignored. The columns are
        /// looked up on the first row and kept with the statement for the rows after it.
        static expected<T> read(const statement& row) noexcept(
            detail::fields_read_noexcept<
                T,
                typename detail::field_member_types<typename detail::field_list<T>::type>::type>::value
        )
        {
            static_assert(
                std::is_default_constructible<T>::value,
                "a struct with fields must be default-constructible: sqlnx makes it, then fills its fields; specialize "
                "record_traits (with record_builder) for a type that needs constructor arguments"
            );
            using list_type = typename detail::field_list<T>::type;
            const list_type list = sqlnx_fields(static_cast<const T*>(nullptr));
            expected<const int*> columns = detail::column_lookup::find<T>(row, list);
            if (!columns)
            {
                return expected<T>(unexpect, std::move(columns.error()));
            }
            T value{};
            expected<void> filled = fill<0>(
                row,
                *columns,
                value,
                list,
                std::integral_constant<bool, std::tuple_size<list_type>::value == 0>()
            );
            if (!filled)
            {
                return expected<T>(unexpect, std::move(filled.error()));
            }
            return expected<T>(std::move(value));
        }

        /// Binds every parameter of the statement to the field of its name, without the prefix (":name", "@name" or
        /// "$name" all take the field name), compared exactly, as SQLite compares parameter names. A parameter without
        /// a field, or a positional one (?), is a primary_code::range error, so none is left NULL by accident; fields
        /// without a parameter are left out.
        template <class Target>
        static expected<void> bind(
            Target& stmt,
            const T& value
        ) noexcept(detail::
                       fields_bind_noexcept<
                           typename detail::field_member_types<typename detail::field_list<T>::type>::type>::value)
        {
            using list_type = typename detail::field_list<T>::type;
            const list_type list = sqlnx_fields(static_cast<const T*>(nullptr));
            const int parameters = stmt.native_statement().bind_parameter_count();
            for (int index = 1; index <= parameters; ++index)
            {
                const char* name = stmt.native_statement().bind_parameter_name(index);
                if (name == nullptr || name[0] == '?')
                {
                    return expected<void>(unexpect, detail::positional_parameter(index));
                }
                bool found = false;
                expected<void> bound = bind_field<0>(
                    stmt,
                    index,
                    name + 1,
                    value,
                    list,
                    found,
                    std::integral_constant<bool, std::tuple_size<list_type>::value == 0>()
                );
                if (!bound)
                {
                    return bound;
                }
                if (!found)
                {
                    return expected<void>(unexpect, detail::parameter_without_field(name));
                }
            }
            return {};
        }

    private:
        template <
            std::size_t I,
            class Target,
            class List>
        static expected<void> bind_field(
            Target&,
            int,
            const char*,
            const T&,
            const List&,
            bool&,
            std::true_type /*past the end*/
        ) noexcept
        {
            return {};
        }

        /// Binds the first field named `name` to parameter `index`, and sets `found`
        template <
            std::size_t I,
            class Target,
            class List>
        static expected<void> bind_field(
            Target& stmt,
            int index,
            const char* name,
            const T& value,
            const List& list,
            bool& found,
            std::false_type /*past the end*/
        ) noexcept(detail::fields_bind_noexcept<typename detail::field_member_types<List>::type>::value)
        {
            const typename std::tuple_element<I, List>::type& field = std::get<I>(list);
            if (std::strcmp(field.name, name) == 0)
            {
                found = true;
                return stmt.bind_at(index, value.*(field.member));
            }
            return bind_field<I + 1>(
                stmt,
                index,
                name,
                value,
                list,
                found,
                std::integral_constant<bool, I + 1 == std::tuple_size<List>::value>()
            );
        }

        template <
            std::size_t I,
            class List>
        static expected<void> fill(
            const statement&,
            const int*,
            T&,
            const List&,
            std::true_type /*past the end*/
        ) noexcept
        {
            return {};
        }

        /// Fills field I from its kept column, or from the column it looks up when none are kept (out of memory)
        template <
            std::size_t I,
            class List>
        static expected<void> fill(
            const statement& row,
            const int* columns,
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
            expected<int> index = columns != nullptr ? expected<int>(columns[I]) : row.column_index(field.name);
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
            return fill<I + 1>(
                row,
                columns,
                value,
                list,
                std::integral_constant<bool, I + 1 == std::tuple_size<List>::value>()
            );
        }
    };
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_DETAIL_FIELD_RECORD_HPP
