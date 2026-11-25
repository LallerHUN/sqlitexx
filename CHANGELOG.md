# Changelog

All notable changes to sqlitexx are listed here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and sqlitexx follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html): before 1.0, a minor version may break the API.

## [Unreleased]

## [0.4.0] - 2026-11-25

### Added

- `database::create_collation(name, compare)`: collations written in C++, for `COLLATE`, `ORDER BY` and indexes.
  `compare` takes two `string_view`s and returns a negative, zero or positive number, or in C++20 what `operator<=>`
  gives. Native layer: `connection::create_collation`.
- `database::remove_function(name, argument_count)` and `remove_collation(name)` take back SQL functions, aggregates
  and collations. SQLite releases its copy at once, so they can refer to state outside them that ends earlier than
  the database.
- `registrations`: a token that removes the functions, aggregates and collations registered with it when it ends.
  `create_function`, `create_aggregate` and `create_collation` take it as their last parameter. `remove()` removes
  now and reports errors, `release()` keeps the registrations. When the token can't remove a registration (`busy`),
  it ends the callable: functions called afterwards fail with `misuse`, collations end the program.
- Hooks: `database::commit_hook`, `rollback_hook`, `update_hook` and `progress_handler`, with or without a
  `registrations` token. A commit hook returning false turns the commit into a rollback, a progress handler returning
  false interrupts the statement. `nullptr` clears a hook. Native layer: `connection::commit_hook`, `rollback_hook`,
  `update_hook`, `progress_handler` and `native::update_kind`.

## [0.3.0] - 2026-11-25

### Added

- `database::create_function(name, callable, flags)`: SQL functions written in C++. Arguments and results go through
  `value_traits`; an `expected<T>` result or (with exceptions enabled) an exception fails the call with its code and
  message. `native::function_flags` marks a function `deterministic`, `directonly` or `innocuous`.
- Native layer: `connection::create_function`, `native::value` for arguments and `native::context` for results.
- `database::create_aggregate(name, prototype, flags)`: aggregate functions written in C++, from a class with
  `step(...)` and `finish()`; each group works on its own copy of the prototype. A class that also has `inverse(...)`
  and `value()` is a window function too. Native layer: `connection::create_aggregate`,
  `connection::create_window_function` and `context::aggregate_context`.
- `database::backup_to(destination)` copies a whole database over another one, for example to save an in-memory
  database to a file. `begin_backup_to(destination)` gives a `backup` that copies in steps with progress. Native
  layer: `native::backup`.
- `database::open_blob(table, column, rowid, access)` gives a `blob_stream` for reading and writing a blob in pieces,
  with `read`, `write`, `seek` and `reopen`. `write` takes any contiguous container of bytes; `read` sizes a
  growable container to what was read, or fills a fixed-size buffer without allocating. Both take an optional
  `count`: the most bytes to read, or the first bytes to write. `zeroblob{size}` binds a blob of zero bytes to
  reserve the space without building it in memory. Native layer: `native::blob_handle`.
- With exceptions enabled, running out of memory in sqlitexx is a `primary_code::nomem` error instead of
  `std::bad_alloc`: copying text and blobs, building error messages, and allocating the copies of what
  `create_function` and `create_aggregate` register. Exceptions from user code pass through unchanged.

### Changed

- The built-in `value_traits` take the statement as a template parameter, so they serve SQL functions as well. Traits
  written for `native::statement` keep working with statements.
- The calls that run no user code are `noexcept`: `database::open`, `execute`, `prepare`, `busy_timeout`,
  `backup_to`, `begin_backup_to`, `open_blob`, `statement::step`, `reset` and `column_index`, `backup::step`, the
  `blob_stream` calls that take a pointer, and `transaction::begin`, `commit` and `rollback`. The built-in
  `value_traits` are `noexcept`, and `statement::bind`, `bind_at`, `bind_named`, `execute`, `column`, `read_row` and
  `first` are `noexcept` when the `value_traits` of their types are. `create_function` and `create_aggregate` are
  `noexcept` when moving what they register cannot throw.
- `create_function`, `create_aggregate`, `bind_values`, `execute_with`, `for_each_row` and `on()` take what they keep
  by value and move it in, so an lvalue is copied at the call, in the caller's code.

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

[Unreleased]: https://github.com/LallerHUN/sqlitexx/compare/v0.4.0...develop
[0.4.0]: https://github.com/LallerHUN/sqlitexx/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/LallerHUN/sqlitexx/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/LallerHUN/sqlitexx/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/LallerHUN/sqlitexx/releases/tag/v0.1.0
