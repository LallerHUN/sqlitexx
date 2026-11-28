#ifndef SQLNX_SQLITE_ROWS_HPP
#define SQLNX_SQLITE_ROWS_HPP

/// \file
/// The rows of a query as a range:
///
///     for (sqlnx::sqlite::expected<std::tuple<std::string, int>>& row : select.rows<std::string, int>())
///     {
///         if (!row)
///         {
///             // row.error(): reading this row failed, or the query itself failed
///             break;
///         }
///         use(std::get<0>(*row), std::get<1>(*row));
///     }
///
/// Each element is the expected<row<T...>> that read_row<T...>() gives, so an error can't go unnoticed. When reading a
/// row fails (NULL into a non-optional type, an integer that doesn't fit), only that row is an error, and iteration may
/// go on. When stepping fails, that error is the last element. Iteration starts where the statement is: rows() steps
/// without resetting it, like first() and for_each().
///
/// It is a single-pass input range: the element lives in the range, so begin() runs the query once and every iterator
/// sees the current row. The range refers to its statement, so the statement must outlive it, and the range must stay
/// where it is while it is being iterated. In C++20 it is a std::ranges::input_range: views such as std::views::take
/// and std::views::filter work on it.

#include "sqlnx/sqlite/statement.hpp"

#include <cstddef>
#include <iterator>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    template <class... T>
    class results;

    template <class... T>
    class rows_range
    {
    public:
        using value_type = expected<row<T...>>;

        /// What iterator++ gives: the row it moved past, kept alive for *it++.
        class postfix_value
        {
        public:
            explicit postfix_value(value_type&& passed)
                : value(std::move(passed))
            {
            }

            value_type& operator*() noexcept
            {
                return value;
            }

        private:
            value_type value;
        };

        class iterator
        {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = expected<row<T...>>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            /// A default-constructed iterator is the end.
            iterator() noexcept = default;

            reference operator*() const noexcept
            {
                return *range->current;
            }

            pointer operator->() const noexcept
            {
                return &*range->current;
            }

            iterator& operator++()
            {
                range->advance();
                return *this;
            }

            postfix_value operator++(int)
            {
                postfix_value passed(std::move(*range->current));
                range->advance();
                return passed;
            }

            /// Single pass: iterators differ only in whether they are at the end.
            friend bool operator==(
                const iterator& left,
                const iterator& right
            ) noexcept
            {
                return left.at_end() == right.at_end();
            }

            friend bool operator!=(
                const iterator& left,
                const iterator& right
            ) noexcept
            {
                return left.at_end() != right.at_end();
            }

        private:
            rows_range* range = nullptr;

            friend class rows_range;

            explicit iterator(rows_range* source) noexcept
                : range(source)
            {
            }

            bool at_end() const noexcept
            {
                return range == nullptr || !range->current;
            }
        };

        /// Lifetime: movable before iteration starts, never copied (two copies would step one statement).
        rows_range(rows_range&&) noexcept = default;
        rows_range& operator=(rows_range&&) noexcept = default;
        rows_range(const rows_range&) = delete;
        rows_range& operator=(const rows_range&) = delete;

        /// Iteration. The first call to begin() steps to the first row; later calls give an iterator at the current
        /// row.
        iterator begin()
        {
            if (!started)
            {
                started = true;
                if (!finished)
                {
                    advance();
                }
            }
            return iterator(this);
        }

        iterator end() noexcept
        {
            return iterator();
        }

    private:
        statement* stmt = nullptr;
        optional<value_type> current;
        bool started = false;
        bool finished = false;

        friend class statement;
        friend class results<T...>;

        explicit rows_range(statement& source) noexcept
            : stmt(&source)
        {
        }

        /// A query that couldn't start: the error is the only element
        explicit rows_range(error&& failed) noexcept
            : finished(true)
        {
            // Built in place: moving a temporary expected in makes GCC warn about its unused value
            // (-Wmaybe-uninitialized)
            current.emplace(unexpect, std::move(failed));
        }

        void advance()
        {
            if (finished)
            {
                current = nullopt;
                return;
            }
            expected<bool> stepped = stmt->step();
            if (!stepped)
            {
                // The last element: the query cannot go on
                current.emplace(unexpect, std::move(stepped.error()));
                finished = true;
                return;
            }
            if (!*stepped)
            {
                current = nullopt;
                finished = true;
                return;
            }
            current = stmt->template read_row<T...>();
        }
    };

    template <class... T>
    rows_range<T...> statement::rows() &
    {
        return rows_range<T...>(*this);
    }
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_ROWS_HPP
