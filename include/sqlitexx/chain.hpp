#ifndef SQLITEXX_CHAIN_HPP
#define SQLITEXX_CHAIN_HPP

// Building blocks for chaining expected<...> results:
//
//     auto age = db.prepare("SELECT age FROM user WHERE name = ?")
//         .and_then(sqlitexx::bind_values("Alice"))
//         .and_then(sqlitexx::first<int>())
//         .or_else(sqlitexx::on(native::primary_code::done, [](const sqlitexx::error&) {
//             return sqlitexx::expected<int>(0); // no such user
//         }));
//
// bind_values / first / for_each_row / execute_with are steps for and_then on an expected<statement>. on() handles
// one kind of error in or_else and passes every other error on unchanged.

#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
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
        explicit bind_step(Args... values)
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

    template <class F, class... T>
    class for_each_step
    {
    public:
        explicit for_each_step(F f)
            : callback(std::move(f))
        {
        }

        expected<void> operator()(statement& stmt)
        {
            return stmt.for_each<T...>(callback);
        }

        expected<void> operator()(statement&& stmt)
        {
            return stmt.for_each<T...>(callback);
        }

    private:
        F callback;
    };

    template <class... Args>
    class execute_step
    {
    public:
        explicit execute_step(Args... values)
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

    struct primary_code_match
    {
        native::primary_code code;

        bool operator()(const error& e) const noexcept
        {
            return e.primary() == code;
        }
    };

    struct extended_code_match
    {
        int code;

        bool operator()(const error& e) const noexcept
        {
            return e.code().code() == code;
        }
    };

    template <class Match, class F>
    class error_handler
    {
    public:
        error_handler(
            Match matcher,
            F f
        )
            : match(matcher),
              handler(std::move(f))
        {
        }

        // The handler's return type is the chain's type, so a non-matching error is passed on in that type.
        template <class E>
        auto operator()(E&& e) const -> decltype(std::declval<const F&>()(std::declval<const error&>()))
        {
            using result_type = decltype(std::declval<const F&>()(std::declval<const error&>()));
            if (match(e))
            {
                return handler(e);
            }
            return result_type(unexpect, std::forward<E>(e));
        }

    private:
        Match match;
        F handler;
    };
} // detail

// and_then step: binds the values to parameters 1, 2, 3, ... and passes the statement on.
template <class... Args>
detail::bind_step<typename std::decay<Args>::type...> bind_values(Args&&... values)
{
    return detail::bind_step<typename std::decay<Args>::type...>(std::forward<Args>(values)...);
}

// and_then step: steps once and reads the row (statement::first).
template <class... T>
detail::first_step<T...> first()
{
    return detail::first_step<T...>();
}

// and_then step: calls `callback` with every row (statement::for_each).
template <
    class... T,
    class F>
detail::for_each_step<
    typename std::decay<F>::type,
    T...>
for_each_row(F&& callback)
{
    return detail::for_each_step<typename std::decay<F>::type, T...>(std::forward<F>(callback));
}

// and_then step: binds the values and runs the statement to completion (statement::execute).
template <class... Args>
detail::execute_step<typename std::decay<Args>::type...> execute_with(Args&&... values)
{
    return detail::execute_step<typename std::decay<Args>::type...>(std::forward<Args>(values)...);
}

// or_else handler for errors with this primary code (any extended code under it); other errors pass through.
template <class F>
detail::error_handler<
    detail::primary_code_match,
    typename std::decay<F>::type>
on(native::primary_code code,
   F&& handler)
{
    return detail::error_handler<detail::primary_code_match, typename std::decay<F>::type>(
        detail::primary_code_match{code},
        std::forward<F>(handler)
    );
}

// or_else handler for errors with exactly this extended code, for example SQLITE_CONSTRAINT_UNIQUE.
template <class F>
detail::error_handler<
    detail::extended_code_match,
    typename std::decay<F>::type>
on(int extended_code,
   F&& handler)
{
    return detail::error_handler<detail::extended_code_match, typename std::decay<F>::type>(
        detail::extended_code_match{extended_code},
        std::forward<F>(handler)
    );
}

} // sqlitexx

#endif // SQLITEXX_CHAIN_HPP
