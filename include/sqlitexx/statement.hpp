#ifndef SQLITEXX_STATEMENT_HPP
#define SQLITEXX_STATEMENT_HPP

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/noexcept_traits.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/statement.hpp"
#include "sqlitexx/value.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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

/// What reading a row gives: the value itself for one column, a std::tuple for several.
template <class... T>
using row = typename detail::row_type<T...>::type;

template <class... T>
class rows_range;

namespace detail
{
    class column_lookup;
} // detail

/// A prepared statement with typed binding and reading. Move-only. Every operation that can fail returns
/// expected<...>, so calls chain with and_then / or_else / transform. Running out of memory is a primary_code::nomem
/// error, never std::bad_alloc. The calls that bind or read values are noexcept when the value_traits of their types
/// are, as the built-in ones are; exceptions from other traits pass through unchanged (see sqlitexx/value.hpp).
class statement final
{
public:
    statement() noexcept = default;

    explicit statement(native::statement&& prepared) noexcept;

    statement(statement&&) noexcept = default;
    statement& operator=(statement&&) noexcept = default;

    statement(const statement&) = delete;
    statement& operator=(const statement&) = delete;

    /// \name Binding
    /// bind() binds its arguments to parameters 1, 2, 3, ... in order, or a record (sqlitexx/record_traits.hpp), on its
    /// own, to the parameters by name. On a temporary statement it gives the statement back, ready for the next step
    /// in a chain.
    ///@{
    template <class... Args>
    SQLITEXX_DETAIL_NODISCARD expected<void> bind(const Args&... args) & noexcept(
        detail::all_bind_noexcept<Args...>::value
    )
    {
        return bind_all(args...);
    }

    template <class... Args>
    SQLITEXX_DETAIL_NODISCARD expected<statement> bind(const Args&... args) && noexcept(
        detail::all_bind_noexcept<Args...>::value
    )
    {
        expected<void> bound = bind_all(args...);
        if (!bound)
        {
            return expected<statement>(unexpect, std::move(bound.error()));
        }
        return expected<statement>(std::move(*this));
    }

    template <class T>
    SQLITEXX_DETAIL_NODISCARD expected<void> bind_at(
        int index,
        const T& value
    ) noexcept(detail::binds_noexcept<T>::value)
    {
        return check(value_traits<typename std::decay<T>::type>::bind(stmt, index, value));
    }
    ///@}

    /// The name includes its prefix: ":id", "@id" or "$id".
    template <class T>
    SQLITEXX_DETAIL_NODISCARD expected<void> bind_named(
        const char* name,
        const T& value
    ) noexcept(detail::binds_noexcept<T>::value)
    {
        const int index = stmt.bind_parameter_index(name);
        if (index == 0)
        {
            return expected<void>(unexpect, no_parameter(name));
        }
        return bind_at(index, value);
    }

    /// \name Running
    /// step() gives true when a row is ready and false when the statement has finished.
    ///@{
    SQLITEXX_DETAIL_NODISCARD expected<bool> step() noexcept;
    SQLITEXX_DETAIL_NODISCARD expected<void> reset() noexcept;
    ///@}

    /// Resets, binds the arguments (all earlier bindings are cleared first) and runs to completion; rows are
    /// discarded. Suits INSERT, UPDATE, DELETE and DDL, and runs the same statement again with new values.
    template <class... Args>
    SQLITEXX_DETAIL_NODISCARD expected<void> execute(const Args&... args) noexcept(
        detail::all_bind_noexcept<Args...>::value
    )
    {
        restart();
        expected<void> bound = bind_all(args...);
        if (!bound)
        {
            return bound;
        }
        for (;;)
        {
            expected<bool> stepped = step();
            if (!stepped)
            {
                return expected<void>(unexpect, std::move(stepped.error()));
            }
            if (!*stepped)
            {
                return {};
            }
        }
    }

    /// Steps once and reads the row. A query that produces no row gives a primary_code::done error.
    template <class... T>
    SQLITEXX_DETAIL_NODISCARD expected<row<T...>> first() noexcept(detail::row_reads_noexcept<T...>::value)
    {
        expected<bool> stepped = step();
        if (!stepped)
        {
            return expected<row<T...>>(unexpect, std::move(stepped.error()));
        }
        if (!*stepped)
        {
            return expected<row<T...>>(unexpect, no_rows());
        }
        return read_row<T...>();
    }

    /// Steps through all rows and calls `callback` with each row's values: callback(T1, T2, ...).
    template <
        class... T,
        class F>
    SQLITEXX_DETAIL_NODISCARD expected<void> for_each(F&& callback)
    {
        for (;;)
        {
            expected<bool> stepped = step();
            if (!stepped)
            {
                return expected<void>(unexpect, std::move(stepped.error()));
            }
            if (!*stepped)
            {
                return {};
            }
            expected<row<T...>> values = read_row<T...>();
            if (!values)
            {
                return expected<void>(unexpect, std::move(values.error()));
            }
            call_with_row(callback, *values, std::integral_constant<bool, sizeof...(T) == 1>());
        }
    }

    /// The rows as a range of expected<row<T...>>, for a range-based for loop (see sqlitexx/rows.hpp). Not on a
    /// temporary statement: it would be gone before the loop body runs.
    template <class... T>
    rows_range<T...> rows() &;

    template <class... T>
    void rows() && = delete;

    /// Reading the current row (indexes count from 0)
    template <class T>
    SQLITEXX_DETAIL_NODISCARD expected<T> column(int index) const noexcept(detail::reads_noexcept<T>::value)
    {
        if (!stmt)
        {
            return expected<T>(unexpect, empty_statement());
        }
        return read_column<T>(index, detail::is_optional<T>());
    }

    /// By name: the first column with this name, compared like SQL identifiers (ASCII letters in either case). A
    /// column's name is its AS alias, or else what SQLite makes of the expression, so give computed columns an alias.
    /// Each call searches the columns: in a loop, look the index up once with column_index().
    template <class T>
    SQLITEXX_DETAIL_NODISCARD expected<T> column(string_view name) const noexcept(detail::reads_noexcept<T>::value)
    {
        expected<int> index = column_index(name);
        if (!index)
        {
            return expected<T>(unexpect, std::move(index.error()));
        }
        return column<T>(*index);
    }

    /// The index of the first column with this name; a primary_code::range error when there is none.
    SQLITEXX_DETAIL_NODISCARD expected<int> column_index(string_view name) const noexcept;

    /// The current row: the value for one column, a std::tuple for several, or a record (sqlitexx/record_traits.hpp),
    /// such as a struct whose fields SQLITEXX_FIELDS names.
    template <class... T>
    SQLITEXX_DETAIL_NODISCARD expected<row<T...>> read_row() const noexcept(detail::row_reads_noexcept<T...>::value)
    {
        static_assert(
            sizeof...(T) == 1 || !detail::any_is_record<T...>::value,
            "a record is read on its own: read_row<my_record>(), not alongside other types"
        );
        if (!stmt)
        {
            return expected<row<T...>>(unexpect, empty_statement());
        }
        return read_row_impl<T...>(std::integral_constant<bool, sizeof...(T) == 1>());
    }

    /// The native statement underneath, for what the typed layer does not cover
    native::statement& native_statement() noexcept;
    const native::statement& native_statement() const noexcept;

private:
    /// The columns a record's fields read (sqlitexx/fields.hpp), found once and kept while the statement keeps its
    /// prepared form: a schema change prepares it again, and the count of those tells the kept columns are stale.
    struct record_columns
    {
        const void* record;
        int reprepares;
        std::vector<int> indexes;
    };

    native::statement stmt;
    mutable record_columns field_columns{nullptr, 0, std::vector<int>()};

    friend class detail::column_lookup;

    /// Errors
    error failure(native::result code) const noexcept;
    error no_rows() const noexcept;
    error no_parameter(const char* name) const noexcept;
    error no_column(string_view name) const noexcept;
    error null_column(int index) const noexcept;
    /// Reading from a statement that holds none: default-constructed, or moved from
    error empty_statement() const noexcept;

    expected<void> check(native::result code) const noexcept
    {
        if (code.failed())
        {
            return expected<void>(unexpect, failure(code));
        }
        return {};
    }

    /// Binding
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
    ) noexcept(detail::
                   all_bind_noexcept<
                       Arg,
                       Rest...>::value)
    {
        expected<void> bound = bind_at(index, arg);
        if (!bound)
        {
            return bound;
        }
        return bind_from(index + 1, rest...);
    }

    /// Binding all arguments: a record on its own binds through its record_traits, anything else by position
    template <class... Args>
    expected<void> bind_all(const Args&... args) noexcept(detail::all_bind_noexcept<Args...>::value)
    {
        static_assert(
            sizeof...(Args) == 1 || !detail::any_is_record<Args...>::value,
            "a record binds on its own: bind(my_record), not alongside other values"
        );
        return bind_record_or_values(
            std::integral_constant<bool, sizeof...(Args) == 1 && detail::any_is_record<Args...>::value>(),
            args...
        );
    }

    template <class... Args>
    expected<void> bind_record_or_values(
        std::false_type /*record*/,
        const Args&... args
    ) noexcept(detail::all_bind_noexcept<Args...>::value)
    {
        return bind_from(1, args...);
    }

    template <class Record>
    expected<void> bind_record_or_values(
        std::true_type /*record*/,
        const Record& value
    ) noexcept(detail::binds_noexcept<Record>::value)
    {
        static_assert(
            detail::is_bind_record<Record>::value,
            "this record has no record_traits<T>::bind(): it can be read, but not bound; name its fields with "
            "SQLITEXX_FIELDS or add bind() to its record_traits"
        );
        return bind_record(value, detail::is_bind_record<Record>());
    }

    template <class Record>
    expected<void> bind_record(
        const Record& value,
        std::true_type /*binds*/
    ) noexcept(detail::binds_noexcept<Record>::value)
    {
        return record_traits<Record>::bind(*this, value);
    }

    /// Only reached after the static_assert above has failed; keeps the compiler to that one message
    template <class Record>
    expected<void> bind_record(
        const Record&,
        std::false_type /*binds*/
    ) noexcept
    {
        return {};
    }

    /// Reading
    template <class T>
    expected<T> read_column(
        int index,
        std::false_type /*is_optional*/
    ) const noexcept(detail::reads_noexcept<T>::value)
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
    ) const noexcept(detail::reads_noexcept<T>::value)
    {
        return value_traits<T>::read(stmt, index);
    }

    template <class T>
    expected<T> read_row_impl(std::true_type /*single column*/) const noexcept(detail::reads_noexcept<T>::value)
    {
        return read_single<T>(detail::is_record<T>());
    }

    template <class T>
    expected<T> read_single(std::false_type /*record*/) const noexcept(detail::reads_noexcept<T>::value)
    {
        return column<T>(0);
    }

    /// A record: read by its record_traits
    template <class T>
    expected<T> read_single(std::true_type /*record*/) const noexcept(detail::reads_noexcept<T>::value)
    {
        static_assert(
            !detail::has_read_traits<T>::value,
            "this type has both value_traits and record_traits (or fields): drop one, so it is clear whether it fills "
            "one column or a row"
        );
        return record_traits<T>::read(*this);
    }

    template <class... T>
    expected<std::tuple<T...>> read_row_impl(std::false_type /*single column*/) const noexcept(
        detail::row_reads_noexcept<T...>::value
    )
    {
        std::tuple<T...> values;
        expected<void> filled = read_into<0>(values, std::integral_constant<bool, sizeof...(T) == 0>());
        if (!filled)
        {
            return expected<std::tuple<T...>>(unexpect, std::move(filled.error()));
        }
        return expected<std::tuple<T...>>(std::move(values));
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
            return expected<void>(unexpect, std::move(value.error()));
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

// The record_traits of SQLITEXX_FIELDS structs, and rows(), which returns a rows_range that needs the complete
// statement above
#include "sqlitexx/detail/field_record.hpp"
#include "sqlitexx/rows.hpp"

#endif // SQLITEXX_STATEMENT_HPP
