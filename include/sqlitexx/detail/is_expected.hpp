#ifndef SQLITEXX_DETAIL_IS_EXPECTED_HPP
#define SQLITEXX_DETAIL_IS_EXPECTED_HPP

/// \file
/// Whether a type is an expected<T>, for callables that may return either a value or an expected.

#include "sqlitexx/expected.hpp"

#include <type_traits>

namespace sqlitexx
{
namespace detail
{
    template <class T>
    struct is_expected : std::false_type
    {
    };

    template <class T>
    struct is_expected<expected<T>> : std::true_type
    {
    };

    /// What a call that returns R gives as an expected: R itself when it is one, expected<R> otherwise
    template <class R, bool = is_expected<R>::value>
    struct as_expected
    {
        using type = expected<R>;
    };

    template <class R>
    struct as_expected<R, true>
    {
        using type = R;
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_IS_EXPECTED_HPP
