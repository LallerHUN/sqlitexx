# Changelog

All notable changes to sqlitexx are listed here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and sqlitexx follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html): before 1.0, a minor version may break the API.

## [Unreleased]

### Added

- `database::create_function(name, callable, flags)`: SQL functions written in C++. Arguments and results go through
  `value_traits`; an `expected<T>` result or (with exceptions enabled) an exception fails the call with its code and
  message. `native::function_flags` marks a function `deterministic`, `directonly` or `innocuous`.
- Native layer: `connection::create_function`, `native::value` for arguments and `native::context` for results.

### Changed

- The built-in `value_traits` take the statement as a template parameter, so they serve SQL functions as well. Traits
  written for `native::statement` keep working with statements.

## [0.2.0] - 2026-11-23

### Added

- C++11 and C++14 support (MSVC from C++14, its oldest mode). Before C++17, `string_view` and `optional` come from
  string-view-lite 1.8.2 and optional-lite 3.6.0, chosen once at configure time with `SQLITEXX_CXX17_TYPES`;
  `SQLITEXX_USE_SYSTEM_LITE_TYPES` uses installed copies.
- `statement::rows<T...>()`: the rows of a query as a range of `expected<row<T...>>`, for a range-based `for` loop.
  In C++20 it is a `std::ranges::input_range`.
- `native::extended_code`: all 80 extended result codes by name (`constraint_unique` for
  `SQLITE_CONSTRAINT_UNIQUE`), for `on()` and for comparing with `error::code()`, without `sqlite3.h`.
- `statement::column<T>(name)` and `statement::column_index(name)`: columns by name, compared like SQL identifiers.
- `database::busy_timeout(std::chrono::milliseconds)`: wait for another connection's lock instead of failing with
  `busy` at once.
- `cstring_view`, `u16cstring_view` and (C++20) `u8cstring_view`: views that are always NUL-terminated. They bind as
  text and read text columns without a copy.
- `std::byte` blobs (C++17), and in C++20 `std::span` blobs and `char8_t` text: `u8"..."`, `std::u8string`,
  `std::u8string_view`.
- `tools/check.sh` runs CI's Linux jobs locally under Linux or WSL.

### Changed

- **Breaking:** `native::statement::column_text()` and `column_text16()` return `optional<cstring_view>` and
  `optional<u16cstring_view>` instead of `string_view` and `u16string_view`. They are empty where SQLite returns
  NULL (a NULL value, or out of memory), and the text is always NUL-terminated.
- **Breaking:** when SQLite runs out of memory converting a column to text, the typed layer gives a `nomem` error
  instead of an empty string.
- The error for NULL read into a non-optional type names the column: `column 2 (age) is NULL`.
- CI requires 23 checks, now covering C++11 and C++14 on GCC, Clang (with ASan and UBSan) and MSVC.

## [0.1.0] - 2026-11-23

### Added

- Typed layer: `database` and `statement` with positional, named and variadic binding, `first<T...>()` and
  `for_each<T...>()`, all returning `expected<T, error>`.
- Chaining helpers for `and_then` (`bind_values`, `first`, `for_each_row`, `execute_with`) and `on(code, handler)`
  for `or_else`.
- Strict values: integers range-checked in both directions, NULL only into `optional<T>`, and `prepare()` rejects
  SQL holding more than one statement. Own types plug in through `value_traits`.
- Transactions: `in_transaction(db, body)` and the RAII `transaction`.
- Native layer: a one-to-one, no-exception RAII mirror of the SQLite C API. Public headers don't include
  `sqlite3.h`.
- Builds and passes its tests with exceptions disabled.
- `std::expected` from C++23, expected-lite 0.10.0 before it, chosen once at configure time.
- CMake package for `find_package(sqlitexx)`, `FetchContent` and `add_subdirectory`. SQLite 3.54.1 is bundled, or
  the system SQLite is used with `SQLITEXX_USE_SYSTEM_SQLITE=ON`.

[Unreleased]: https://github.com/LallerHUN/sqlitexx/compare/v0.2.0...develop
[0.2.0]: https://github.com/LallerHUN/sqlitexx/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/LallerHUN/sqlitexx/releases/tag/v0.1.0
