#ifndef SQLITEXX_EXPECTED_HPP
#define SQLITEXX_EXPECTED_HPP

// expected<T, E> for the typed API: std::expected or expected-lite, as CMake chose when sqlitexx was configured (see
// SQLITEXX_EXPECTED in CMakeLists.txt). Use these names, not std:: or nonstd:: ones, and code keeps compiling whichever
// was chosen.

#include "sqlitexx/config.hpp"
#include "sqlitexx/error.hpp"

#if !defined(SQLITEXX_USE_STD_EXPECTED)
// Linking the sqlitexx CMake target defines it. Without CMake, define it the way the library was built: 0 for
// expected-lite, 1 for std::expected.
#error "SQLITEXX_USE_STD_EXPECTED is not defined: link the sqlitexx CMake target (see sqlitexx/expected.hpp)"
#endif

#if SQLITEXX_USE_STD_EXPECTED

#if SQLITEXX_CPLUSPLUS < 202302L
#error "This sqlitexx was built with std::expected, so code using it needs C++23 as well"
#endif

#include <expected>

namespace sqlitexx
{
template <class T, class E = error>
using expected = std::expected<T, E>;

template <class E>
using unexpected = std::unexpected<E>;

using std::unexpect;
using std::unexpect_t;
} // sqlitexx

#else

#include <nonstd/expected.hpp>

namespace sqlitexx
{
template <class T, class E = error>
using expected = nonstd::expected<T, E>;

// expected-lite names it unexpected only from C++17 on; unexpected_type works for every standard.
template <class E>
using unexpected = nonstd::unexpected_type<E>;

using nonstd::unexpect;
using nonstd::unexpect_t;
} // sqlitexx

#endif

#endif // SQLITEXX_EXPECTED_HPP
