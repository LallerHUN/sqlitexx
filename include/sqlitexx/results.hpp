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
/// then. next(count) is the exception: it reads on from where the previous next() stopped, a page at a time. Rows are
/// read one step at a time; nothing is cached or read ahead.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/detail/reads_view.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/statement.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

/// The static_assert message for collecting views from several rows
#define SQLITEXX_DETAIL_VIEWS_IN_VECTOR \
    "each step ends the views read from the row before it, so a vector of them would dangle: read std::string or a " \
    "blob instead of string_view, cstring_view or std::span"

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
    /// first() runs the query and reads its first row: a primary_code::done error when there is none. On a temporary,
    /// as in db.query<T>(sql).first(), the statement ends before the row is returned, so types that read as a view
    /// into it (string_view, cstring_view, std::span) don't compile there; on a named results they stay valid until
    /// its next run.
    ///
    /// all() reads every row into a vector, stopping at the first error. Views never compile with it: each step ends
    /// the views of the row before.
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

    SQLITEXX_DETAIL_NODISCARD expected<std::vector<row<T...>>> all()
    {
        static_assert(!detail::any_reads_view<T...>::value, SQLITEXX_DETAIL_VIEWS_IN_VECTOR);
        using all_rows = expected<std::vector<row<T...>>>;
        std::vector<row<T...>> rows;
        for (value_type& element : *this)
        {
            if (!element)
            {
                return all_rows(unexpect, std::move(element.error()));
            }
            if (!append(rows, element))
            {
                return all_rows(unexpect, detail::out_of_memory());
            }
        }
        return all_rows(std::move(rows));
    }
    ///@}

    /// \name Pages
    /// next(count) reads up to `count` rows from where the previous next() stopped; the first call, and the first
    /// after a loop, first() or all(), starts the query from the beginning. An empty vector means there are no more
    /// rows, and further calls stay empty until something else runs the query again.
    ///
    ///     sqlitexx::results<std::string> names = db.query<std::string>("SELECT name FROM user ORDER BY name");
    ///     for (;;)
    ///     {
    ///         sqlitexx::expected<std::vector<std::string>> page = names.next(20);
    ///         if (!page || page->empty())
    ///         {
    ///             break;
    ///         }
    ///         show(*page);
    ///     }
    ///
    /// A row that can't be read (NULL into a non-optional type) makes the call return that error, and the rows of the
    /// page read before it are dropped; the next call goes on after that row. When stepping fails, the error ends the
    /// pages. Views never compile here, as with all().
    ///@{
    SQLITEXX_DETAIL_NODISCARD expected<std::vector<row<T...>>> next(std::size_t count)
    {
        static_assert(!detail::any_reads_view<T...>::value, SQLITEXX_DETAIL_VIEWS_IN_VECTOR);
        using page_rows = expected<std::vector<row<T...>>>;
        if (!source)
        {
            return page_rows(unexpect, copied_error());
        }
        if (paging == page_state::closed)
        {
            rewind();
            paging = page_state::open;
        }
        std::vector<row<T...>> rows;
        while (paging == page_state::open && rows.size() < count)
        {
            expected<bool> stepped = source->step();
            if (!stepped)
            {
                paging = page_state::finished;
                return page_rows(unexpect, std::move(stepped.error()));
            }
            if (!*stepped)
            {
                paging = page_state::finished;
                break;
            }
            value_type element = source->template read_row<T...>();
            if (!element)
            {
                return page_rows(unexpect, std::move(element.error()));
            }
            if (!append(rows, element))
            {
                return page_rows(unexpect, detail::out_of_memory());
            }
        }
        return page_rows(std::move(rows));
    }
    ///@}

private:
    /// Where next() stands: not started (or interrupted by another run), reading pages, or past the last row
    enum class page_state
    {
        closed,
        open,
        finished
    };

    /// The statement, bound and ready, or why it isn't
    expected<statement> source;
    /// The current loop's range; its iterators point into it
    optional<rows_range<T...>> pass;
    page_state paging = page_state::closed;

    friend class database;

    explicit results(expected<statement>&& prepared) noexcept
        : source(std::move(prepared))
    {
    }

    /// Back to the start, keeping the bindings. reset() reports how the previous run ended, which that run already
    /// gave to its reader. Every run but next()'s own interrupts the pages.
    void rewind() noexcept
    {
        static_cast<void>(source->reset());
        paging = page_state::closed;
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

    /// Moves the row in; false when that runs out of memory
    static bool append(
        std::vector<row<T...>>& rows,
        value_type& element
    )
    {
#if SQLITEXX_DETAIL_EXCEPTIONS
        try
        {
            rows.push_back(std::move(*element));
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
#else
        rows.push_back(std::move(*element));
#endif
        return true;
    }
};
} // sqlitexx

#endif // SQLITEXX_RESULTS_HPP
