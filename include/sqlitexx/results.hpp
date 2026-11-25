#ifndef SQLITEXX_RESULTS_HPP
#define SQLITEXX_RESULTS_HPP

/// \file
/// What database::query() gives: the rows of a query, read as you go.
///
///     for (sqlitexx::expected<std::tuple<std::string, int>>& user :
///          db.query<std::string, int>("SELECT name, age FROM user WHERE age >= ?", 18))
///     {
///         if (!user)
///         {
///             // user.error(): the query failed, or reading this row did
///             break;
///         }
///         use(std::get<0>(*user), std::get<1>(*user));
///     }
///
///     sqlitexx::expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM user").first();
///
/// results owns its statement, so the loop above is safe in every standard: a range-based for keeps the object it
/// iterates alive. Each element is an expected<row<T...>>, like rows(): when preparing or binding failed, that error
/// is the only element; when reading a row fails, only that row is an error; when stepping fails, that error is the
/// last element.
///
/// Every loop, first() and all() runs the query from the start, with the same arguments, and sees the data as it is
/// then. Rows are read one step at a time; nothing is cached or read ahead.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/detail/reads_view.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/statement.hpp"

#include <type_traits>
#include <utility>
#include <vector>

namespace sqlitexx
{
class database;

/// The rows of a query, from database::query<T...>(). Move-only; it must stay where it is while a loop runs over it.
template <class... T>
class results final
{
public:
    using value_type = expected<row<T...>>;
    using iterator = typename rows_range<T...>::iterator;

    results(results&&) noexcept = default;
    results& operator=(results&&) noexcept = default;
    results(const results&) = delete;
    results& operator=(const results&) = delete;

    /// \name Iteration
    /// begin() runs the query from the start; elements are as in rows().
    ///@{
    iterator begin()
    {
        if (!source)
        {
            pass.emplace(rows_range<T...>(copied_error()));
        }
        else
        {
            rewind();
            pass.emplace(source->template rows<T...>());
        }
        return pass->begin();
    }

    iterator end() noexcept
    {
        return iterator();
    }
    ///@}

    /// \name Reading at once
    /// first() runs the query and reads its first row: a primary_code::done error when there is none. all() reads
    /// every row into a vector, stopping at the first error. On a temporary, as in db.query<T>(sql).first(), the
    /// statement ends before the row is returned, so types that read as a view into it (string_view, cstring_view,
    /// std::span) don't compile there; on a named results they stay valid until its next run.
    ///@{
    SQLITEXX_DETAIL_NODISCARD value_type first() &
    {
        return read_first();
    }

    SQLITEXX_DETAIL_NODISCARD value_type first() &&
    {
        static_assert(!detail::any_reads_view<T...>::value, SQLITEXX_DETAIL_VIEW_WOULD_DANGLE);
        return read_first();
    }

    SQLITEXX_DETAIL_NODISCARD expected<std::vector<row<T...>>> all() &
    {
        return read_all();
    }

    SQLITEXX_DETAIL_NODISCARD expected<std::vector<row<T...>>> all() &&
    {
        static_assert(!detail::any_reads_view<T...>::value, SQLITEXX_DETAIL_VIEW_WOULD_DANGLE);
        return read_all();
    }
    ///@}

private:
    /// The statement, bound and ready, or why it isn't
    expected<statement> source;
    /// The current loop's range; its iterators point into it
    optional<rows_range<T...>> pass;

    friend class database;

    explicit results(expected<statement>&& prepared) noexcept
        : source(std::move(prepared))
    {
    }

    /// Back to the start, keeping the bindings. reset() reports how the previous run ended, which that run already
    /// gave to its reader.
    void rewind() noexcept
    {
        static_cast<void>(source->reset());
    }

    error copied_error() const noexcept
    {
#if SQLITEXX_DETAIL_EXCEPTIONS
        try
        {
            return source.error();
        }
        catch (const std::bad_alloc&)
        {
            return detail::out_of_memory();
        }
#else
        return source.error();
#endif
    }

    value_type read_first()
    {
        if (!source)
        {
            return value_type(unexpect, copied_error());
        }
        rewind();
        return source->template first<T...>();
    }

    expected<std::vector<row<T...>>> read_all()
    {
        using all_rows = expected<std::vector<row<T...>>>;
        std::vector<row<T...>> rows;
        for (value_type& element : *this)
        {
            if (!element)
            {
                return all_rows(unexpect, std::move(element.error()));
            }
#if SQLITEXX_DETAIL_EXCEPTIONS
            try
            {
                rows.push_back(std::move(*element));
            }
            catch (const std::bad_alloc&)
            {
                return all_rows(unexpect, detail::out_of_memory());
            }
#else
            rows.push_back(std::move(*element));
#endif
        }
        return all_rows(std::move(rows));
    }
};
} // sqlitexx

#endif // SQLITEXX_RESULTS_HPP
