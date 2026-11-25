#ifndef SQLITEXX_DETAIL_COLLATION_CALL_HPP
#define SQLITEXX_DETAIL_COLLATION_CALL_HPP

// What SQLite calls for a collation made from a C++ callable: it hands the callable both texts as string_views and
// turns what it returns into the negative, zero or positive int SQLite expects.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/utility.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

#if SQLITEXX_CPLUSPLUS >= 202002L && defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

namespace sqlitexx
{
namespace detail
{
    // What a comparison may return: a signed integer, negative, zero or positive like std::string::compare, or (C++20)
    // std::strong_ordering or std::weak_ordering, as operator<=> gives. bool is no comparison: a less-than predicate
    // can't say "equal".
    template <class R, class = void>
    struct collation_order
    {
        static const bool valid = false;
    };

    template <class R>
    struct collation_order<R, typename std::enable_if<std::is_integral<R>::value && std::is_signed<R>::value>::type>
    {
        static const bool valid = true;

        static int sign(R order) noexcept
        {
            return order < 0 ? -1 : (order > 0 ? 1 : 0);
        }
    };

#if defined(__cpp_lib_three_way_comparison)
    template <class R>
    struct collation_order<
        R,
        typename std::enable_if<
            std::is_same<R, std::strong_ordering>::value || std::is_same<R, std::weak_ordering>::value>::type>
    {
        static const bool valid = true;

        static int sign(R order) noexcept
        {
            return order < 0 ? -1 : (order > 0 ? 1 : 0);
        }
    };
#endif

    // The result of calling F with two string_views; no `type` when it can't be called that way
    template <class F, class = void>
    struct collation_result
    {
    };

    template <class F>
    struct collation_result<
        F,
        typename make_void<
            decltype(std::declval<F&>()(std::declval<string_view>(), std::declval<string_view>()))>::type>
    {
        using type = typename std::decay<
            decltype(std::declval<F&>()(std::declval<string_view>(), std::declval<string_view>()))>::type;
    };

    template <class F, class = void>
    struct is_collation : std::false_type
    {
    };

    template <class F>
    struct is_collation<F, typename make_void<typename collation_result<F>::type>::type>
        : std::integral_constant<bool, collation_order<typename collation_result<F>::type>::valid>
    {
    };

    template <class F>
    class collation_caller
    {
    public:
        using order = collation_order<typename collation_result<F>::type>;

        // noexcept: SQLite can't take an error from a comparison, and nothing may unwind through it, so an exception
        // from the callable ends the program
        static int compare(
            void* stored,
            int first_size,
            const void* first,
            int second_size,
            const void* second
        ) noexcept
        {
            F& function = *static_cast<F*>(stored);
            return order::sign(function(
                string_view(static_cast<const char*>(first), static_cast<std::size_t>(first_size)),
                string_view(static_cast<const char*>(second), static_cast<std::size_t>(second_size))
            ));
        }

        static void destroy(void* stored) noexcept
        {
            delete static_cast<F*>(stored);
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_COLLATION_CALL_HPP
