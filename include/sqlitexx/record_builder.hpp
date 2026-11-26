#ifndef SQLITEXX_RECORD_BUILDER_HPP
#define SQLITEXX_RECORD_BUILDER_HPP

/// \file
/// Reading a record's columns one after the other, for record_traits::read(), without an error check after each.
///
///     static sqlitexx::expected<point> read(const sqlitexx::statement& row)
///     {
///         return sqlitexx::record_builder(row)
///             .column<double>("x")       // by name, compared like SQL identifiers
///             .column<double>(1)         // or by index, from 0
///             .build<point>();           // point(x, y), or point{x, y} for an aggregate
///     }
///
/// Each column<T>() reads one more column through value_traits and keeps its value. The first error wins: the
/// columns after it aren't read, and build() gives that error. build<T>() makes a T from the values in order;
/// build(factory) calls factory(values...) and gives what it returns, which may itself be an expected, to reject a
/// row with an error of its own:
///
///     .build([](double x, double y) -> sqlitexx::expected<point> { ... });
///
/// The steps are for one chain: each takes the builder as an rvalue. Inside a template, write
/// `.template column<T>(...)`.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/is_expected.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
template <class Source, class... Values>
class basic_record_builder;

/// Starts reading the row that `row` (a statement) is on
template <class Source>
basic_record_builder<Source> record_builder(const Source& row) noexcept;

/// The values read so far, or the first error. Made by record_builder().
template <class Source, class... Values>
class basic_record_builder final
{
public:
    /// \name Reading
    ///@{
    template <class T>
    SQLITEXX_DETAIL_NODISCARD basic_record_builder<
        Source,
        Values...,
        T>
    column(string_view name) &&
    {
        if (!state)
        {
            return basic_record_builder<Source, Values..., T>(row, std::move(state.error()));
        }
        expected<int> index = row->column_index(name);
        if (!index)
        {
            return basic_record_builder<Source, Values..., T>(row, std::move(index.error()));
        }
        return std::move(*this).template column<T>(*index);
    }

    template <class T>
    SQLITEXX_DETAIL_NODISCARD basic_record_builder<
        Source,
        Values...,
        T>
    column(int index) &&
    {
        if (!state)
        {
            return basic_record_builder<Source, Values..., T>(row, std::move(state.error()));
        }
        expected<T> value = row->template column<T>(index);
        if (!value)
        {
            return basic_record_builder<Source, Values..., T>(row, std::move(value.error()));
        }
        return basic_record_builder<Source, Values..., T>(
            row,
            std::tuple_cat(std::move(*state), std::tuple<T>(std::move(*value)))
        );
    }
    ///@}

    /// \name Building
    ///@{
    template <class T>
    SQLITEXX_DETAIL_NODISCARD expected<T> build() &&
    {
        if (!state)
        {
            return expected<T>(unexpect, std::move(state.error()));
        }
        return expected<T>(make<T>(
            std::move(*state),
            detail::make_index_sequence<sizeof...(Values)>(),
            std::is_constructible<T, Values...>()
        ));
    }

    template <class Factory>
    SQLITEXX_DETAIL_NODISCARD auto build(Factory&& factory) && -> typename detail::as_expected<decltype(detail::apply(
        std::forward<Factory>(factory),
        std::declval<std::tuple<Values...>>()
    ))>::type
    {
        using made = decltype(detail::apply(std::forward<Factory>(factory), std::declval<std::tuple<Values...>>()));
        static_assert(!std::is_void<made>::value, "build(factory): the factory must return the record");
        using result = typename detail::as_expected<made>::type;
        if (!state)
        {
            return result(unexpect, std::move(state.error()));
        }
        return result(detail::apply(std::forward<Factory>(factory), std::move(*state)));
    }
    ///@}

private:
    const Source* row;
    expected<std::tuple<Values...>> state;

    template <class OtherSource, class... OtherValues>
    friend class basic_record_builder;

    template <class OtherSource>
    friend basic_record_builder<OtherSource> record_builder(const OtherSource& row) noexcept;

    basic_record_builder(
        const Source* source,
        std::tuple<Values...>&& values
    )
        : row(source),
          state(std::move(values))
    {
    }

    basic_record_builder(
        const Source* source,
        error&& failed
    ) noexcept
        : row(source),
          state(
              unexpect,
              std::move(failed)
          )
    {
    }

    template <
        class T,
        std::size_t... I>
    static T make(
        std::tuple<Values...>&& values,
        detail::index_sequence<I...>,
        std::true_type /*constructible*/
    )
    {
        static_cast<void>(values); // unused when there are no values
        return T(std::get<I>(std::move(values))...);
    }

    template <
        class T,
        std::size_t... I>
    static T make(
        std::tuple<Values...>&& values,
        detail::index_sequence<I...>,
        std::false_type /*constructible*/
    )
    {
        static_cast<void>(values); // unused when there are no values
        return T{std::get<I>(std::move(values))...};
    }
};

template <class Source>
basic_record_builder<Source> record_builder(const Source& row) noexcept
{
    return basic_record_builder<Source>(&row, std::tuple<>());
}
} // sqlitexx

#endif // SQLITEXX_RECORD_BUILDER_HPP
