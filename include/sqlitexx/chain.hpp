#ifndef SQLITEXX_CHAIN_HPP
#define SQLITEXX_CHAIN_HPP

/// \file
/// Building blocks for chaining expected<...> results:
///
///     auto age = db.prepare("SELECT age FROM user WHERE name = ?")
///         .and_then(sqlitexx::bind_values("Alice"))
///         .and_then(sqlitexx::first<int>())
///         .or_else(sqlitexx::on(native::primary_code::done, [](const sqlitexx::error&) {
///             return sqlitexx::expected<int>(0); // no such user
///         }));
///
/// bind_values / first / for_each_row / execute_with are steps for and_then on an expected<statement>. on() handles
/// one kind of error in or_else (a native::primary_code, or a native::extended_code such as constraint_unique) and
/// passes every other error on unchanged.
///
/// The steps own what they are given: bind_values, execute_with, for_each_row and on() take it by value and move it in,
/// so a step never refers to anything outside itself. Passing an lvalue copies it where the call is made, in the
/// caller's code; the calls themselves only move, and are noexcept when moving cannot throw.

#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/extended_code.hpp"
#include "sqlitexx/native/result.hpp"
#include "sqlitexx/statement.hpp"

#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    template <class... Args>
    class bind_step
    {
    public:
        explicit bind_step(Args... values) noexcept(all_of<std::is_nothrow_move_constructible<Args>...>::value)
            : args(std::move(values)...)
        {
        }

        expected<statement> operator()(statement&& stmt) const
        {
            return detail::apply(binder{stmt}, args);
        }

    private:
        struct binder
        {
            statement& stmt;

            template <class... Values>
            expected<statement> operator()(const Values&... values) const
            {
                return std::move(stmt).bind(values...);
            }
        };

        std::tuple<Args...> args;
    };

    template <class... T>
    struct first_step
    {
        expected<row<T...>> operator()(statement& stmt) const
        {
            return stmt.first<T...>();
        }

        expected<row<T...>> operator()(statement&& stmt) const
        {
            return stmt.first<T...>();
        }
    };

    template <class Callback, class... T>
    struct for_each_step
    {
        Callback callback;

        expected<void> operator()(statement& stmt)
        {
            return stmt.for_each<T...>(callback);
        }

        expected<void> operator()(statement&& stmt)
        {
            return stmt.for_each<T...>(callback);
        }
    };

    template <class... Args>
    class execute_step
    {
    public:
        explicit execute_step(Args... values) noexcept(all_of<std::is_nothrow_move_constructible<Args>...>::value)
            : args(std::move(values)...)
        {
        }

        expected<void> operator()(statement& stmt) const
        {
            return detail::apply(executor{stmt}, args);
        }

        expected<void> operator()(statement&& stmt) const
        {
            return detail::apply(executor{stmt}, args);
        }

    private:
        struct executor
        {
            statement& stmt;

            template <class... Values>
            expected<void> operator()(const Values&... values) const
            {
                return stmt.execute(values...);
            }
        };

        std::tuple<Args...> args;
    };

    /// Predicates for error_handler
    struct has_primary_code
    {
        native::primary_code code;

        bool operator()(const error& e) const noexcept
        {
            return e.primary() == code;
        }
    };

    struct has_extended_code
    {
        int code;

        bool operator()(const error& e) const noexcept
        {
            return e.code().code() == code;
        }
    };

    /// Calls `handler` for errors `predicate` accepts and passes every other error on
    template <class Predicate, class Handler>
    struct error_handler
    {
        Predicate predicate;
        Handler handler;

        /// The handler's return type is the chain's type, so a non-matching error is passed on in that type.
        template <class E>
        auto operator()(E&& e) const -> decltype(std::declval<const Handler&>()(std::declval<const error&>()))
        {
            using result_type = decltype(std::declval<const Handler&>()(std::declval<const error&>()));
            if (predicate(e))
            {
                return handler(e);
            }
            return result_type(unexpect, std::forward<E>(e));
        }
    };
} // detail

/// and_then step: binds the values to parameters 1, 2, 3, ... and passes the statement on.
template <class... Args>
SQLITEXX_DETAIL_NODISCARD detail::bind_step<Args...> bind_values(Args... values) noexcept(
    detail::all_of<std::is_nothrow_move_constructible<Args>...>::value
)
{
    return detail::bind_step<Args...>(std::move(values)...);
}

/// and_then step: steps once and reads the row (statement::first).
template <class... T>
SQLITEXX_DETAIL_NODISCARD detail::first_step<T...> first()
{
    return detail::first_step<T...>();
}

/// and_then step: calls `callback` with every row (statement::for_each).
template <
    class... T,
    class Callback>
SQLITEXX_DETAIL_NODISCARD detail::for_each_step<
    Callback,
    T...>
for_each_row(Callback callback) noexcept(std::is_nothrow_move_constructible<Callback>::value)
{
    return detail::for_each_step<Callback, T...>{std::move(callback)};
}

/// and_then step: binds the values and runs the statement to completion (statement::execute).
template <class... Args>
SQLITEXX_DETAIL_NODISCARD detail::execute_step<Args...> execute_with(Args... values) noexcept(
    detail::all_of<std::is_nothrow_move_constructible<Args>...>::value
)
{
    return detail::execute_step<Args...>(std::move(values)...);
}

/// or_else handler for errors with this primary code (any extended code under it); other errors pass through.
template <class Handler>
SQLITEXX_DETAIL_NODISCARD detail::error_handler<
    detail::has_primary_code,
    Handler>
on(native::primary_code code,
   Handler handler) noexcept(std::is_nothrow_move_constructible<Handler>::value)
{
    return detail::error_handler<detail::has_primary_code, Handler>{
        detail::has_primary_code{code},
        std::move(handler),
    };
}

/// or_else handler for errors with exactly this extended code, for example native::extended_code::constraint_unique.
template <class Handler>
SQLITEXX_DETAIL_NODISCARD detail::error_handler<
    detail::has_extended_code,
    Handler>
on(native::extended_code code,
   Handler handler) noexcept(std::is_nothrow_move_constructible<Handler>::value)
{
    return detail::error_handler<detail::has_extended_code, Handler>{
        detail::has_extended_code{static_cast<int>(code)},
        std::move(handler),
    };
}

/// or_else handler for errors with exactly this extended code given as a number, for a code that
/// native::extended_code doesn't list yet.
template <class Handler>
SQLITEXX_DETAIL_NODISCARD detail::error_handler<
    detail::has_extended_code,
    Handler>
on(int extended_code,
   Handler handler) noexcept(std::is_nothrow_move_constructible<Handler>::value)
{
    return detail::error_handler<detail::has_extended_code, Handler>{
        detail::has_extended_code{extended_code},
        std::move(handler),
    };
}

} // sqlitexx

#endif // SQLITEXX_CHAIN_HPP
