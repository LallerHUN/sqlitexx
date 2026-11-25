#ifndef SQLITEXX_DETAIL_FUNCTION_CALL_HPP
#define SQLITEXX_DETAIL_FUNCTION_CALL_HPP

// What SQLite calls for an SQL function made from a C++ callable: it reads the arguments with value_traits, calls the
// callable and turns what it returns into the result or the error of the call.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/sql_call.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/native/context.hpp"

#include <tuple>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    template <class F>
    class function_caller
    {
    public:
        using arguments = typename callable_signature<F>::arguments;

        static const int argument_count = static_cast<int>(std::tuple_size<arguments>::value);

        static void call(
            sqlite3_context* handle,
            int,
            sqlite3_value** values
        ) noexcept
        {
            native::context ctx(handle);
            guarded(
                ctx,
                [&]
                {
                    arguments read;
                    if (!argument_reader<arguments>::read(ctx, values, read))
                    {
                        return;
                    }
                    F& function = *static_cast<F*>(ctx.user_data());
                    result_writer::respond(
                        ctx,
                        [&]
                        {
                            return detail::apply(function, std::move(read));
                        }
                    );
                }
            );
        }

        static void destroy(void* stored) noexcept
        {
            delete static_cast<F*>(stored);
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_FUNCTION_CALL_HPP
