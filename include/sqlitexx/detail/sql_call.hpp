#ifndef SQLITEXX_DETAIL_SQL_CALL_HPP
#define SQLITEXX_DETAIL_SQL_CALL_HPP

// What every C++ callback SQLite runs needs, for SQL functions and aggregates alike: reading the arguments with
// value_traits, turning what the C++ code returns into the result or the error of the call, and keeping exceptions
// from reaching SQLite.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/function_values.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/context.hpp"
#include "sqlitexx/native/result.hpp"
#include "sqlitexx/value.hpp"

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#if SQLITEXX_DETAIL_EXCEPTIONS
#include <exception>
#endif

namespace sqlitexx
{
namespace detail
{
    // The parameters of a callable with one signature: a function pointer, a member function pointer, or a class with a
    // single, non-template operator() (a lambda without auto parameters)
    template <class F>
    struct callable_signature : callable_signature<decltype(&F::operator())>
    {
    };

    template <class R, class... A>
    struct callable_signature<R (*)(A...)>
    {
        using arguments = std::tuple<typename std::decay<A>::type...>;
    };

    template <class C, class R, class... A>
    struct callable_signature<R (C::*)(A...)> : callable_signature<R (*)(A...)>
    {
    };

    template <class C, class R, class... A>
    struct callable_signature<R (C::*)(A...) const> : callable_signature<R (*)(A...)>
    {
    };

#if defined(__cpp_noexcept_function_type)
    template <class R, class... A>
    struct callable_signature<R (*)(A...) noexcept> : callable_signature<R (*)(A...)>
    {
    };

    template <class C, class R, class... A>
    struct callable_signature<R (C::*)(A...) noexcept> : callable_signature<R (*)(A...)>
    {
    };

    template <class C, class R, class... A>
    struct callable_signature<R (C::*)(A...) const noexcept> : callable_signature<R (*)(A...)>
    {
    };
#endif

    template <class T>
    struct is_expected : std::false_type
    {
    };

    template <class T>
    struct is_expected<expected<T>> : std::true_type
    {
    };

    // The call failed: the statement running it fails with this error's code and message
    inline void fail(
        native::context& ctx,
        const error& e
    )
    {
        ctx.result_error(e.message());
        // A code that is not an error would make SQLite carry on as if the call had succeeded
        ctx.result_error_code(
            e.code().failed() ? e.code() : native::result(static_cast<int>(native::primary_code::error))
        );
    }

    // Runs `body` and turns an exception into the error of the call
    template <class Body>
    void guarded(
        native::context& ctx,
        Body&& body
    ) noexcept
    {
#if SQLITEXX_DETAIL_EXCEPTIONS
        try
        {
            body();
        }
        catch (const std::bad_alloc&)
        {
            ctx.result_error_nomem();
        }
        catch (const std::exception& e)
        {
            ctx.result_error(e.what());
        }
        catch (...)
        {
            ctx.result_error("the SQL function threw an exception");
        }
#else
        (void)ctx;
        body();
#endif
    }

    // Arguments: read into a tuple of the parameter types; NULL only into optional<T>
    template <class Arguments>
    class argument_reader
    {
    public:
        // False when an argument doesn't fit; the call has failed then
        static bool read(
            native::context& ctx,
            sqlite3_value** values,
            Arguments& read
        )
        {
            expected<void> filled = read_into<0>(
                argument_source(values),
                read,
                std::integral_constant<bool, std::tuple_size<Arguments>::value == 0>()
            );
            if (!filled)
            {
                fail(ctx, filled.error());
                return false;
            }
            return true;
        }

    private:
        template <std::size_t I>
        static expected<void> read_into(
            const argument_source&,
            Arguments&,
            std::true_type /*past the end*/
        )
        {
            return {};
        }

        template <std::size_t I>
        static expected<void> read_into(
            const argument_source& source,
            Arguments& read,
            std::false_type /*past the end*/
        )
        {
            using element = typename std::tuple_element<I, Arguments>::type;
            expected<element> value = read_argument<element>(source, static_cast<int>(I), is_optional<element>());
            if (!value)
            {
                return expected<void>(unexpect, value.error());
            }
            std::get<I>(read) = std::move(*value);
            return read_into<I + 1>(
                source,
                read,
                std::integral_constant<bool, I + 1 == std::tuple_size<Arguments>::value>()
            );
        }

        template <class T>
        static expected<T> read_argument(
            const argument_source& source,
            int index,
            std::false_type /*is_optional*/
        )
        {
            if (source.column_type(index) == native::value_type::null)
            {
                return expected<T>(
                    unexpect,
                    error(
                        native::result(static_cast<int>(native::primary_code::mismatch)),
                        value_name(source, index) + " is NULL; take it as an optional to accept NULL"
                    )
                );
            }
            return value_traits<T>::read(source, index);
        }

        template <class T>
        static expected<T> read_argument(
            const argument_source& source,
            int index,
            std::true_type /*is_optional*/
        )
        {
            return value_traits<T>::read(source, index);
        }
    };

    // Results: a value becomes the result, expected<T> an error or its value, void NULL
    class result_writer
    {
    public:
        template <class Call>
        static void respond(
            native::context& ctx,
            Call&& call
        )
        {
            using returned_type = decltype(call());
            respond(ctx, call, std::is_void<returned_type>(), is_expected<returned_type>());
        }

        // For step() and inverse(), which return nothing or expected<void>: false when the call failed
        template <class Call>
        static bool proceed(
            native::context& ctx,
            Call&& call
        )
        {
            using returned_type = decltype(call());
            return proceed(ctx, call, std::is_void<returned_type>());
        }

    private:
        template <class Call>
        static void respond(
            native::context& ctx,
            Call& call,
            std::true_type /*void*/,
            std::false_type /*expected*/
        )
        {
            call();
            ctx.result_null();
        }

        template <class Call>
        static void respond(
            native::context& ctx,
            Call& call,
            std::false_type /*void*/,
            std::false_type /*expected*/
        )
        {
            set_result(ctx, call());
        }

        template <class Call>
        static void respond(
            native::context& ctx,
            Call& call,
            std::false_type /*void*/,
            std::true_type /*expected*/
        )
        {
            auto returned = call();
            if (!returned)
            {
                fail(ctx, returned.error());
                return;
            }
            set_value(ctx, returned);
        }

        template <class Call>
        static bool proceed(
            native::context&,
            Call& call,
            std::true_type /*void*/
        )
        {
            call();
            return true;
        }

        template <class Call>
        static bool proceed(
            native::context& ctx,
            Call& call,
            std::false_type /*void*/
        )
        {
            static_assert(
                std::is_same<decltype(call()), expected<void>>::value,
                "step() and inverse() return void or expected<void>"
            );
            expected<void> returned = call();
            if (!returned)
            {
                fail(ctx, returned.error());
                return false;
            }
            return true;
        }

        template <class T>
        static void set_value(
            native::context& ctx,
            const expected<T>& returned
        )
        {
            set_result(ctx, *returned);
        }

        static void set_value(
            native::context& ctx,
            const expected<void>&
        )
        {
            ctx.result_null();
        }

        template <class T>
        static void set_result(
            native::context& ctx,
            const T& value
        )
        {
            result_target target(ctx);
            const native::result code = value_traits<typename std::decay<T>::type>::bind(target, 0, value);
            if (code.failed())
            {
                fail(ctx, error(code, std::string("the SQL function's result cannot be stored: ") + code.errstr()));
            }
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_SQL_CALL_HPP
