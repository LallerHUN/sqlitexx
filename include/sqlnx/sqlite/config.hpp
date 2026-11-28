#ifndef SQLNX_SQLITE_CONFIG_HPP
#define SQLNX_SQLITE_CONFIG_HPP

/// \file
/// Build configuration and vocabulary types.
///
/// Every public header gets its vocabulary types (string_view, optional) from here, never from std:: directly, and
/// expected from sqlnx/sqlite/expected.hpp. They are the std:: types from C++17, or the string-view-lite and
/// optional-lite backports, as CMake chose when sqlnx was configured (see SQLNX_CXX17_TYPES in CMakeLists.txt). The
/// choice is made once per build: one choice per translation unit would break the ODR.

#if defined(_MSVC_LANG)
#define SQLNX_CPLUSPLUS _MSVC_LANG
#else
#define SQLNX_CPLUSPLUS __cplusplus
#endif

#if SQLNX_CPLUSPLUS < 201103L
#error "sqlnx needs C++11 or newer"
#endif

// For results that must not be ignored, errors above all: [[nodiscard]] from C++17. Before it, Clang's own attribute,
// which a cast to void silences like [[nodiscard]]; GCC's warn_unused_result isn't used, as no cast silences it.
#if SQLNX_CPLUSPLUS >= 201703L
#define SQLNX_DETAIL_NODISCARD [[nodiscard]]
#elif defined(__clang__)
#define SQLNX_DETAIL_NODISCARD [[clang::warn_unused_result]]
#else
#define SQLNX_DETAIL_NODISCARD
#endif

#if !defined(SQLNX_USE_STD_CXX17_TYPES)
// Linking the sqlnx CMake target defines it. Without CMake, define it the way the library was built: 0 for the
// backports, 1 for the std:: types.
#error "SQLNX_USE_STD_CXX17_TYPES is not defined: link the sqlnx CMake target (see sqlnx/sqlite/config.hpp)"
#endif

#if SQLNX_USE_STD_CXX17_TYPES

#if SQLNX_CPLUSPLUS < 201703L
#error "This sqlnx was built with std::string_view and std::optional, so code using it needs C++17 as well"
#endif

#include <optional>
#include <string_view>

namespace sqlnx
{
namespace sqlite
{
    using std::basic_string_view;
    using std::nullopt;
    using std::optional;
    using std::string_view;
    using std::u16string_view;
} // sqlite
} // sqlnx

#else

#include <nonstd/optional.hpp>
#include <nonstd/string_view.hpp>

namespace sqlnx
{
namespace sqlite
{
    using nonstd::basic_string_view;
    using nonstd::nullopt;
    using nonstd::optional;
    using nonstd::string_view;
    using nonstd::u16string_view;
} // sqlite
} // sqlnx

#endif

#endif // SQLNX_SQLITE_CONFIG_HPP
