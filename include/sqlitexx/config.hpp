#ifndef SQLITEXX_CONFIG_HPP
#define SQLITEXX_CONFIG_HPP

// Build configuration and vocabulary types.
//
// Every public header gets its vocabulary types (string_view, optional, and later span) from here, never from std::
// directly, and expected from sqlitexx/expected.hpp. When C++11/14 support arrives, the std types here get backports
// too, chosen by CMake once per build the way expected already is: one choice per translation unit would break the ODR.

#if defined(_MSVC_LANG)
#define SQLITEXX_CPLUSPLUS _MSVC_LANG
#else
#define SQLITEXX_CPLUSPLUS __cplusplus
#endif

#if SQLITEXX_CPLUSPLUS < 201703L
#error "sqlitexx currently needs C++17 or newer (C++11 and C++14 support is planned)"
#endif

#include <optional>
#include <string_view>

namespace sqlitexx
{
using std::nullopt;
using std::optional;
using std::string_view;
using std::u16string_view;
} // sqlitexx

#endif // SQLITEXX_CONFIG_HPP
