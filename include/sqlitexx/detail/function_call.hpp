#ifndef SQLITEXX_DETAIL_FUNCTION_CALL_HPP
#define SQLITEXX_DETAIL_FUNCTION_CALL_HPP

// What SQLite calls for an SQL function made from a C++ callable: it reads the arguments with value_traits, calls the
// callable and turns what it returns into the result or the error of the call.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/function_values.hpp"
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

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#include <exception>
#include <new>
#define SQLITEXX_FUNCTION_CATCHES 1
#else
#define SQLITEXX_FUNCTION_CATCHES 0
#endif

namespace sqlitexx
{
namespace detail
{
    // The parameters of a callable with one signature: a function pointer, or a class with a single, non-template
    // operator() (a lambda without auto parameters)
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

    template <class F>
    class function_caller
    {
    public:
        using arguments = typename callable_signature<F>::arguments;
        using return_type = decltype(detail::apply(std::declval<F&>(), std::declval<arguments>()));

        static const int argument_count = static_cast<int>(std::tuple_size<arguments>::value);

        static void call(
            sqlite3_context* handle,
            int,
            sqlite3_value** values
        ) noexcept
        {
            native::context ctx(handle);
#if SQLITEXX_FUNCTION_CATCHES
            try
            {
                run(ctx, values);
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
            run(ctx, values);
#endif
        }

        static void destroy(void* stored) noexcept
        {
            delete static_cast<F*>(stored);
        }

    private:
        static void run(
            native::context& ctx,
            sqlite3_value** values
        )
        {
            arguments read;
            expected<void> filled = read_into<0>(
                argument_source(values),
                read,
                std::integral_constant<bool, std::tuple_size<arguments>::value == 0>()
            );
            if (!filled)
            {
                fail(ctx, filled.error());
                return;
            }
            F& function = *static_cast<F*>(ctx.user_data());
            respond(ctx, function, read, std::is_void<return_type>(), is_expected<return_type>());
        }

        // Arguments
        template <std::size_t I>
        static expected<void> read_into(
            const argument_source&,
            arguments&,
            std::true_type /*past the end*/
        )
        {
            return {};
        }

        template <std::size_t I>
        static expected<void> read_into(
            const argument_source& source,
            arguments& read,
            std::false_type /*past the end*/
        )
        {
            using element = typename std::tuple_element<I, arguments>::type;
            expected<element> value = read_argument<element>(source, static_cast<int>(I), is_optional<element>());
            if (!value)
            {
                return expected<void>(unexpect, value.error());
            }
            std::get<I>(read) = std::move(*value);
            return read_into<I + 1>(
                source,
                read,
                std::integral_constant<bool, I + 1 == std::tuple_size<arguments>::value>()
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

        // Results
        static void respond(
            native::context& ctx,
            F& function,
            arguments& read,
            std::true_type /*void*/,
            std::false_type /*expected*/
        )
        {
            detail::apply(function, std::move(read));
            ctx.result_null();
        }

        static void respond(
            native::context& ctx,
            F& function,
            arguments& read,
            std::false_type /*void*/,
            std::false_type /*expected*/
        )
        {
            set_result(ctx, detail::apply(function, std::move(read)));
        }

        static void respond(
            native::context& ctx,
            F& function,
            arguments& read,
            std::false_type /*void*/,
            std::true_type /*expected*/
        )
        {
            return_type returned = detail::apply(function, std::move(read));
            if (!returned)
            {
                fail(ctx, returned.error());
                return;
            }
            set_value(ctx, returned);
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

        static void fail(
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
    };
} // detail
} // sqlitexx

#undef SQLITEXX_FUNCTION_CATCHES

#endif // SQLITEXX_DETAIL_FUNCTION_CALL_HPP
