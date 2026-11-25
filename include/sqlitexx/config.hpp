#ifndef SQLITEXX_CONFIG_HPP
#define SQLITEXX_CONFIG_HPP

/// \file
/// Build configuration and vocabulary types.
///
/// Every public header gets its vocabulary types (string_view, optional) from here, never from std:: directly, and
/// expected from sqlitexx/expected.hpp. They are the std:: types from C++17, or the string-view-lite and optional-lite
/// backports, as CMake chose when sqlitexx was configured (see SQLITEXX_CXX17_TYPES in CMakeLists.txt). The choice is
/// made once per build: one choice per translation unit would break the ODR.

#if defined(_MSVC_LANG)
#define SQLITEXX_CPLUSPLUS _MSVC_LANG
#else
#define SQLITEXX_CPLUSPLUS __cplusplus
#endif

#if SQLITEXX_CPLUSPLUS < 201103L
#error "sqlitexx needs C++11 or newer"
#endif

// For results that must not be ignored, errors above all: [[nodiscard]] from C++17. Before it, Clang's own attribute,
// which a cast to void silences like [[nodiscard]]; GCC's warn_unused_result isn't used, as no cast silences it.
#if SQLITEXX_CPLUSPLUS >= 201703L
#define SQLITEXX_DETAIL_NODISCARD [[nodiscard]]
#elif defined(__clang__)
#define SQLITEXX_DETAIL_NODISCARD [[clang::warn_unused_result]]
#else
#define SQLITEXX_DETAIL_NODISCARD
#endif

#if !defined(SQLITEXX_USE_STD_CXX17_TYPES)
// Linking the sqlitexx CMake target defines it. Without CMake, define it the way the library was built: 0 for the
// backports, 1 for the std:: types.
#error "SQLITEXX_USE_STD_CXX17_TYPES is not defined: link the sqlitexx CMake target (see sqlitexx/config.hpp)"
#endif

#if SQLITEXX_USE_STD_CXX17_TYPES

#if SQLITEXX_CPLUSPLUS < 201703L
#error "This sqlitexx was built with std::string_view and std::optional, so code using it needs C++17 as well"
#endif

#include <optional>
#include <string_view>

namespace sqlitexx
{
using std::basic_string_view;
using std::nullopt;
using std::optional;
using std::string_view;
using std::u16string_view;
} // sqlitexx

#else

#include <nonstd/optional.hpp>
#include <nonstd/string_view.hpp>

namespace sqlitexx
{
using nonstd::basic_string_view;
using nonstd::nullopt;
using nonstd::optional;
using nonstd::string_view;
using nonstd::u16string_view;
} // sqlitexx

#endif

#endif // SQLITEXX_CONFIG_HPP
