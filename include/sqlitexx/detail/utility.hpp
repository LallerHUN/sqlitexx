#ifndef SQLITEXX_DETAIL_UTILITY_HPP
#define SQLITEXX_DETAIL_UTILITY_HPP

/// \file
/// Small metaprogramming helpers that std:: only has from C++14 or C++17 on (index_sequence, apply, void_t).
/// sqlitexx's own versions keep the library code C++11-clean.

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    /// std::void_t, for detecting members: make_void<decltype(...)>::type is void when the expressions are valid
    template <class... T>
    struct make_void
    {
        using type = void;
    };

    template <std::size_t... I>
    struct index_sequence
    {
    };

    template <std::size_t N, std::size_t... I>
    struct make_index_sequence_impl : make_index_sequence_impl<N - 1, N - 1, I...>
    {
    };

    template <std::size_t... I>
    struct make_index_sequence_impl<0, I...>
    {
        using type = index_sequence<I...>;
    };

    template <std::size_t N>
    using make_index_sequence = typename make_index_sequence_impl<N>::type;

    template <
        class F,
        class Tuple,
        std::size_t... I>
    auto apply_impl(
        F&& f,
        Tuple&& t,
        index_sequence<I...>
    ) -> decltype(std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...))
    {
        return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
    }

    /// std::apply
    template <
        class F,
        class Tuple>
    auto apply(
        F&& f,
        Tuple&& t
    )
        -> decltype(apply_impl(
            std::forward<F>(f),
            std::forward<Tuple>(t),
            make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>()
        ))
    {
        return apply_impl(
            std::forward<F>(f),
            std::forward<Tuple>(t),
            make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>()
        );
    }

} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_UTILITY_HPP
