#ifndef SQLITEXX_DETAIL_OUT_OF_MEMORY_HPP
#define SQLITEXX_DETAIL_OUT_OF_MEMORY_HPP

// Running out of memory as an error rather than std::bad_alloc.
//
// With exceptions enabled, sqlitexx catches std::bad_alloc where it allocates and gives an error with
// primary_code::nomem instead. That error's message, "out of memory", is short enough for the standard libraries to
// keep inside the std::string (13 characters; libstdc++, libc++ and MSVC keep up to 15), so making it allocates
// nothing. Without exceptions, a failed allocation ends the program as it does in the standard library.

#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/result.hpp"

#include <utility>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#include <new>
#define SQLITEXX_DETAIL_EXCEPTIONS 1
#else
#define SQLITEXX_DETAIL_EXCEPTIONS 0
#endif

namespace sqlitexx
{
namespace detail
{
    inline native::result nomem_result() noexcept
    {
        return native::result(static_cast<int>(native::primary_code::nomem));
    }

    inline error out_of_memory() noexcept
    {
        return error(nomem_result(), "out of memory");
    }

    // An error with a fixed message; out_of_memory() when copying the message runs out
    inline error error_with(
        native::result code,
        const char* message
    ) noexcept
    {
#if SQLITEXX_DETAIL_EXCEPTIONS
        try
        {
            return error(code, message);
        }
        catch (const std::bad_alloc&)
        {
            return out_of_memory();
        }
#else
        return error(code, message);
#endif
    }

    // An error with the message that build() puts together; out_of_memory() when putting it together runs out
    template <class Build>
    error built_error(
        native::result code,
        Build&& build
    ) noexcept
    {
#if SQLITEXX_DETAIL_EXCEPTIONS
        try
        {
            return error(code, std::forward<Build>(build)());
        }
        catch (const std::bad_alloc&)
        {
            return out_of_memory();
        }
#else
        return error(code, std::forward<Build>(build)());
#endif
    }

    // Runs `body`, which returns an expected; std::bad_alloc from it becomes out_of_memory(). Other exceptions, from
    // the user's own code, pass through.
    template <class Body>
    auto allocation_guard(Body&& body) -> decltype(std::forward<Body>(body)())
    {
#if SQLITEXX_DETAIL_EXCEPTIONS
        using result_type = decltype(std::forward<Body>(body)());
        try
        {
            return std::forward<Body>(body)();
        }
        catch (const std::bad_alloc&)
        {
            return result_type(unexpect, out_of_memory());
        }
#else
        return std::forward<Body>(body)();
#endif
    }
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_OUT_OF_MEMORY_HPP
