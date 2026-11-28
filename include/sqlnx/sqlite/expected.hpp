#ifndef SQLNX_SQLITE_EXPECTED_HPP
#define SQLNX_SQLITE_EXPECTED_HPP

/// \file
/// expected<T, E> for the typed API: std::expected or expected-lite, as CMake chose when sqlnx was configured (see
/// SQLNX_EXPECTED in CMakeLists.txt). Use these names, not std:: or nonstd:: ones, and code keeps compiling
/// whichever was chosen.

#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/error.hpp"

#if !defined(SQLNX_USE_STD_EXPECTED)
// Linking the sqlnx CMake target defines it. Without CMake, define it the way the library was built: 0 for
// expected-lite, 1 for std::expected.
#error "SQLNX_USE_STD_EXPECTED is not defined: link the sqlnx CMake target (see sqlnx/sqlite/expected.hpp)"
#endif

#if SQLNX_USE_STD_EXPECTED

#if SQLNX_CPLUSPLUS < 202302L
#error "This sqlnx was built with std::expected, so code using it needs C++23 as well"
#endif

#include <expected>

namespace sqlnx
{
namespace sqlite
{
    template <class T, class E = error>
    using expected = std::expected<T, E>;

    template <class E>
    using unexpected = std::unexpected<E>;

    using std::unexpect;
    using std::unexpect_t;
} // sqlite
} // sqlnx

#else

#include <nonstd/expected.hpp>

namespace sqlnx
{
namespace sqlite
{
    template <class T, class E = error>
    using expected = nonstd::expected<T, E>;

    /// expected-lite names it unexpected only from C++17 on; unexpected_type works for every standard.
    template <class E>
    using unexpected = nonstd::unexpected_type<E>;

    using nonstd::unexpect;
    using nonstd::unexpect_t;
} // sqlite
} // sqlnx

#endif

#endif // SQLNX_SQLITE_EXPECTED_HPP
