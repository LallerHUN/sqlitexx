#ifndef SQLITEXX_STATEMENT_HPP
#define SQLITEXX_STATEMENT_HPP

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/statement.hpp"
#include "sqlitexx/value.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    template <class... T>
    struct row_type
    {
        using type = std::tuple<T...>;
    };

    template <class T>
    struct row_type<T>
    {
        using type = T;
    };
} // detail

// What reading a row gives: the value itself for one column, a std::tuple for several.
template <class... T>
using row = typename detail::row_type<T...>::type;

// A prepared statement with typed binding and reading. Move-only. Every operation that can fail returns
// expected<...>, so calls chain with and_then / or_else / transform.
class statement final
{
public:
    statement() noexcept = default;

    explicit statement(native::statement&& prepared) noexcept;

    statement(statement&&) noexcept = default;
    statement& operator=(statement&&) noexcept = default;

    statement(const statement&) = delete;
    statement& operator=(const statement&) = delete;

    // Binding. bind() binds its arguments to parameters 1, 2, 3, ... in order. On a temporary statement it gives
    // the statement back, ready for the next step in a chain.
    template <class... Args>
    expected<void> bind(const Args&... args) &
    {
        return bind_from(1, args...);
    }

    template <class... Args>
    expected<statement> bind(const Args&... args) &&
    {
        expected<void> bound = bind_from(1, args...);
        if (!bound)
        {
            return expected<statement>(unexpect, bound.error());
        }
        return expected<statement>(std::move(*this));
    }

    template <class T>
    expected<void> bind_at(
        int index,
        const T& value
    )
    {
        return check(value_traits<typename std::decay<T>::type>::bind(stmt, index, value));
    }

    // The name includes its prefix: ":id", "@id" or "$id".
    template <class T>
    expected<void> bind_named(
        const char* name,
        const T& value
    )
    {
        const int index = stmt.bind_parameter_index(name);
        if (index == 0)
        {
            return expected<void>(unexpect, no_parameter(name));
        }
        return bind_at(index, value);
    }

    // Running. step() gives true when a row is ready and false when the statement has finished.
    expected<bool> step();
    expected<void> reset();

    // Resets, binds the arguments (all earlier bindings are cleared first) and runs to completion; rows are
    // discarded. Suits INSERT, UPDATE, DELETE and DDL, and runs the same statement again with new values.
    template <class... Args>
    expected<void> execute(const Args&... args)
    {
        restart();
        expected<void> bound = bind_from(1, args...);
        if (!bound)
        {
            return bound;
        }
        for (;;)
        {
            expected<bool> stepped = step();
            if (!stepped)
            {
                return expected<void>(unexpect, stepped.error());
            }
            if (!*stepped)
            {
                return {};
            }
        }
    }

    // Steps once and reads the row. A query that produces no row gives a primary_code::done error.
    template <class... T>
    expected<row<T...>> first()
    {
        expected<bool> stepped = step();
        if (!stepped)
        {
            return expected<row<T...>>(unexpect, stepped.error());
        }
        if (!*stepped)
        {
            return expected<row<T...>>(unexpect, no_rows());
        }
        return read_row<T...>();
    }

    // Steps through all rows and calls `callback` with each row's values: callback(T1, T2, ...).
    template <
        class... T,
        class F>
    expected<void> for_each(F&& callback)
    {
        for (;;)
        {
            expected<bool> stepped = step();
            if (!stepped)
            {
                return expected<void>(unexpect, stepped.error());
            }
            if (!*stepped)
            {
                return {};
            }
            expected<row<T...>> values = read_row<T...>();
            if (!values)
            {
                return expected<void>(unexpect, values.error());
            }
            call_with_row(callback, *values, std::integral_constant<bool, sizeof...(T) == 1>());
        }
    }

    // Reading the current row (indexes count from 0)
    template <class T>
    expected<T> column(int index) const
    {
        return read_column<T>(index, detail::is_optional<T>());
    }

    template <class... T>
    expected<row<T...>> read_row() const
    {
        return read_row_impl<T...>(std::integral_constant<bool, sizeof...(T) == 1>());
    }

    // The native statement underneath, for what the typed layer does not cover
    native::statement& native_statement() noexcept;
    const native::statement& native_statement() const noexcept;

private:
    native::statement stmt;

    // Errors
    error failure(native::result code) const;
    error no_rows() const;
    error no_parameter(const char* name) const;
    error null_column(int index) const;

    expected<void> check(native::result code) const
    {
        if (code.failed())
        {
            return expected<void>(unexpect, failure(code));
        }
        return {};
    }

    // Binding
    void restart() noexcept;

    expected<void> bind_from(int) noexcept
    {
        return {};
    }

    template <
        class Arg,
        class... Rest>
    expected<void> bind_from(
        int index,
        const Arg& arg,
        const Rest&... rest
    )
    {
        expected<void> bound = bind_at(index, arg);
        if (!bound)
        {
            return bound;
        }
        return bind_from(index + 1, rest...);
    }

    // Reading
    template <class T>
    expected<T> read_column(
        int index,
        std::false_type /*is_optional*/
    ) const
    {
        if (stmt.column_type(index) == native::value_type::null)
        {
            return expected<T>(unexpect, null_column(index));
        }
        return value_traits<T>::read(stmt, index);
    }

    template <class T>
    expected<T> read_column(
        int index,
        std::true_type /*is_optional*/
    ) const
    {
        return value_traits<T>::read(stmt, index);
    }

    template <class T>
    expected<T> read_row_impl(std::true_type /*single column*/) const
    {
        return column<T>(0);
    }

    template <class... T>
    expected<std::tuple<T...>> read_row_impl(std::false_type /*single column*/) const
    {
        std::tuple<T...> values;
        expected<void> filled = read_into<0>(values, std::integral_constant<bool, sizeof...(T) == 0>());
        if (!filled)
        {
            return expected<std::tuple<T...>>(unexpect, filled.error());
        }
        return values;
    }

    template <
        std::size_t I,
        class Tuple>
    expected<void> read_into(
        Tuple&,
        std::true_type /*past the end*/
    ) const
    {
        return {};
    }

    template <
        std::size_t I,
        class Tuple>
    expected<void> read_into(
        Tuple& values,
        std::false_type /*past the end*/
    ) const
    {
        using element = typename std::tuple_element<I, Tuple>::type;
        expected<element> value = column<element>(static_cast<int>(I));
        if (!value)
        {
            return expected<void>(unexpect, value.error());
        }
        std::get<I>(values) = std::move(*value);
        return read_into<I + 1>(values, std::integral_constant<bool, I + 1 == std::tuple_size<Tuple>::value>());
    }

    template <
        class F,
        class Value>
    static void call_with_row(
        F& callback,
        Value& value,
        std::true_type /*single column*/
    )
    {
        callback(value);
    }

    template <
        class F,
        class Tuple>
    static void call_with_row(
        F& callback,
        Tuple& values,
        std::false_type /*single column*/
    )
    {
        detail::apply(callback, values);
    }
};

} // sqlitexx

#endif // SQLITEXX_STATEMENT_HPP
