#ifndef SQLNX_SQLITE_DETAIL_FUNCTION_CALL_HPP
#define SQLNX_SQLITE_DETAIL_FUNCTION_CALL_HPP

/// \file
/// What SQLite calls for an SQL function made from a C++ callable: it reads the arguments with value_traits, calls the
/// callable and turns what it returns into the result or the error of the call.

#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/detail/registration_block.hpp"
#include "sqlnx/sqlite/detail/sql_call.hpp"
#include "sqlnx/sqlite/detail/utility.hpp"
#include "sqlnx/sqlite/native/context.hpp"

#include <tuple>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        /// Storage: how user_data holds the callable (owned_by_sqlite, or shared_with_token through a registrations
        /// token)
        template <class F, class Storage = owned_by_sqlite<F>>
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
                        F* function = Storage::callable(ctx.user_data());
                        if (function == nullptr)
                        {
                            registration_ended(ctx);
                            return;
                        }
                        arguments read;
                        if (!argument_reader<arguments>::read(ctx, values, read))
                        {
                            return;
                        }
                        result_writer::respond(
                            ctx,
                            [&]
                            {
                                return detail::apply(*function, std::move(read));
                            }
                        );
                    }
                );
            }

            static void destroy(void* stored) noexcept
            {
                Storage::destroy(stored);
            }
        };
    } // detail
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_DETAIL_FUNCTION_CALL_HPP
