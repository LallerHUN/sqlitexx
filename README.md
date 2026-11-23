# sqlitexx

A C++ wrapper for [SQLite](https://sqlite.org) in two layers:

- **A typed layer** where everything that can fail returns `expected<T, error>`. Calls chain with `and_then`,
  `or_else` and `transform`, values are bound and read as C++ types, and NULL and out-of-range integers are errors
  rather than silent zeros.
- **A native layer** that mirrors the SQLite C API one to one: RAII handles, result codes, no exceptions, no
  allocations. Use it where the typed layer doesn't reach, or as a thin, safe base for your own abstractions.

> **Status:** early development. The API may still change between commits.

## Example

```cpp
#include <sqlitexx/sqlitexx.hpp>

#include <cstdio>
#include <string>

namespace sql = sqlitexx;

int main()
{
    // Open: every call that can fail returns expected<T, error>
    sql::expected<sql::database> opened = sql::database::open(":memory:");
    if (!opened)
    {
        std::printf("cannot open: %s\n", opened.error().message().c_str());
        return 1;
    }
    sql::database& db = *opened;

    sql::expected<void> created =
        db.execute("CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, age INTEGER)");
    if (!created)
    {
        std::printf("cannot create: %s\n", created.error().message().c_str());
        return 1;
    }

    // Prepare once, run with different values. optional<T> binds and reads NULL.
    sql::expected<sql::statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(?, ?)");
    if (!insert || !insert->execute("Alice", 31) || !insert->execute("Bob", sql::optional<int>()))
    {
        std::printf("cannot insert\n");
        return 1;
    }

    // Errors carry SQLite's code and message
    sql::expected<void> duplicate = insert->execute("Alice", 32);
    if (!duplicate)
    {
        std::printf("expected failure: %s\n", duplicate.error().message().c_str());
    }

    // Read every row: one column gives the value, several a std::tuple or a callback with one argument per column
    sql::expected<void> listed =
        db.prepare("SELECT name, age FROM user ORDER BY id")
            .and_then(
                sql::for_each_row<std::string, sql::optional<int>>(
                    [](const std::string& name, sql::optional<int> age)
                    {
                        std::printf("%s: %s\n", name.c_str(), age ? std::to_string(*age).c_str() : "unknown");
                    }
                )
            );

    // Chain the steps, and turn one kind of error into a value: no such user gives -1
    sql::expected<int> carol = db.prepare("SELECT age FROM user WHERE name = ?")
                                   .and_then(sql::bind_values("Carol"))
                                   .and_then(sql::first<int>())
                                   .or_else(
                                       sql::on(
                                           sql::native::primary_code::done,
                                           [](const sql::error&)
                                           {
                                               return sql::expected<int>(-1);
                                           }
                                       )
                                   );

    std::printf("Carol: %d\n", carol.value_or(-2));
    return listed && carol && *carol == -1 ? 0 : 1;
}
```

It prints:

```text
expected failure: UNIQUE constraint failed: user.name
Alice: 31
Bob: unknown
Carol: -1
```

This program is [examples/quick_start.cpp](examples/quick_start.cpp); CI builds and runs it on every change.

## Requirements

- A C++17 compiler. Tested in CI with MSVC (Visual Studio 2026), GCC and Clang on Linux, and AppleClang on macOS.
- CMake 3.21 or newer.
- SQLite 3.37 or newer. By default CMake downloads and builds SQLite itself, so nothing needs to be installed.

## Using sqlitexx in your project

With CMake's `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(sqlitexx
    GIT_REPOSITORY https://github.com/LallerHUN/sqlitexx.git
    GIT_TAG develop # pin a commit for reproducible builds
)
FetchContent_MakeAvailable(sqlitexx)

target_link_libraries(my_app PRIVATE sqlitexx::sqlitexx)
```

Or as a subdirectory: `add_subdirectory(sqlitexx)`. Installing with `cmake --install` is not supported yet.

Include everything with `#include <sqlitexx/sqlitexx.hpp>`, or the individual headers.

## Building and testing

```shell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

### CMake options

| Option | Default | Effect |
|---|---|---|
| `SQLITEXX_USE_SYSTEM_SQLITE` | `OFF` | Use an installed SQLite (`find_package(SQLite3)`) instead of downloading the pinned one. Turn it on when your application links SQLite itself: two copies of SQLite in one process can corrupt databases on POSIX systems. |
| `SQLITEXX_EXPECTED` | `AUTO` | Which `expected` the typed API uses: `STD` (C++23 `std::expected`), `LITE` ([expected-lite](https://github.com/nonstd-lite/expected-lite)), or `AUTO`, which picks `std::expected` when `CMAKE_CXX_STANDARD` is 23 or newer and the standard library has it. |
| `SQLITEXX_USE_SYSTEM_EXPECTED_LITE` | `OFF` | Use an installed expected-lite instead of downloading the pinned one. |
| `SQLITEXX_BUILD_TESTS` | top-level only | Build the tests. |
| `SQLITEXX_BUILD_EXAMPLES` | top-level only | Build the examples and run them as tests. |
| `SQLITEXX_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings in sqlitexx and its tests as errors. CI turns it on. |

For an offline build, unpack the dependencies yourself and point CMake at them with
`-DFETCHCONTENT_SOURCE_DIR_SQLITE3=<dir>` and `-DFETCHCONTENT_SOURCE_DIR_EXPECTED_LITE=<dir>`.

## The typed layer

### Values

| C++ type | Binds | Reads |
|---|---|---|
| integers | as a 64-bit integer; values that don't fit are an error | range-checked against the type |
| `bool` | 0 or 1 | non-zero is `true` |
| `float`, `double` | as a double | |
| `std::string`, `string_view` | as text | a `string_view` points into SQLite's memory until the next step |
| `const char*` | as text, or NULL for `nullptr` | not readable |
| `nullptr` | NULL | not readable |
| `sqlitexx::blob` (`std::vector<unsigned char>`) | as a blob | |
| `optional<T>` | NULL when empty | NULL becomes an empty optional |

Reading NULL into anything other than `optional<T>` is an error. Your own types plug in by specializing
`sqlitexx::value_traits`; [sqlitexx/value.hpp](include/sqlitexx/value.hpp) shows how.

### Errors

`sqlitexx::error` holds SQLite's extended result code and its message. Errors sqlitexx finds itself use SQLite codes
too, so one kind of check covers both:

| Primary code | Meaning in sqlitexx |
|---|---|
| `done` | `first()` found no row |
| `mismatch` | NULL read into a non-optional type, or an integer that doesn't fit |
| `misuse` | `prepare()` was given no SQL statement, or more than one |

`sqlitexx::on(code, handler)` handles one kind of error inside `or_else` and passes every other error on unchanged.
`code` is a primary code (`native::primary_code::busy`) or an exact extended code as an `int`, such as the value of
`SQLITE_CONSTRAINT_UNIQUE`. sqlitexx's headers don't include `sqlite3.h`, so the `SQLITE_*` names need your own
`#include <sqlite3.h>`.

### Chaining helpers

| Helper | Use with | Does |
|---|---|---|
| `bind_values(args...)` | `and_then` | binds `args` to parameters 1, 2, 3, ... |
| `first<T...>()` | `and_then` | steps once and reads the row |
| `for_each_row<T...>(callback)` | `and_then` | calls `callback` with every row |
| `execute_with(args...)` | `and_then` | binds `args` and runs the statement to completion |
| `on(code, handler)` | `or_else` | handles errors with that code |

### Transactions

`sqlitexx::in_transaction` runs a function in a transaction: it commits when the function returns a value and rolls
back when it returns an error.

```cpp
sql::expected<std::int64_t> id = sql::in_transaction(db, [&] {
    return db.prepare("INSERT INTO user(name) VALUES(?)")
        .and_then(sql::execute_with("Carol"))
        .transform([&] { return db.last_insert_rowid(); });
});
```

For more control, `sqlitexx::transaction::begin(db, mode)` gives a transaction object that rolls back when it goes
out of scope unless `commit()` succeeded. `mode` is `transaction_mode::deferred` (the default), `immediate` or
`exclusive`, as in SQL's `BEGIN`. When `commit()` fails, `active()` tells whether the transaction is still open: it
is after a deferred constraint violation or `SQLITE_BUSY`, so you can fix the data or retry, while some other errors
make SQLite roll back on its own. A transaction must not outlive its database.

### std::expected or expected-lite

`sqlitexx::expected` is `std::expected` or expected-lite's, depending on `SQLITEXX_EXPECTED`. CMake decides once and
passes the choice to everything that links sqlitexx, so the library and your code always agree on which one the API
uses. Write `sqlitexx::expected`, `sqlitexx::unexpected` and `sqlitexx::unexpect` and your code works with either.

## The native layer

`sqlitexx::native::connection` and `sqlitexx::native::statement` own a `sqlite3*` and a `sqlite3_stmt*`. Their
functions keep the C names (`prepare`, `step`, `bind_int64`, `column_text`, ...), return a `native::result`, and
never throw. Differences from the C API:

- Connections close with `sqlite3_close_v2`, so statements may outlive their connection.
- Extended result codes are always on.
- Text and blobs go in and out as `string_view` and pointer plus size; an empty `string_view` binds an empty text,
  not NULL.
- The public headers don't include `sqlite3.h`.

## Contributing

- Formatting is [.clang-format](.clang-format), checked in CI with clang-format 22.1.3, the version Visual Studio
  2026 ships. Run it before committing.
- Within each `public`, `protected` and `private` section of a class, put types, constants and data members first,
  then the functions, grouped by what they do. clang-format can't check this one.
- Library code stays free of C++17-only features that have no simple C++11 replacement: support for C++11 and C++14 is
  planned.
- CI builds with warnings as errors on MSVC, GCC and Clang, runs the tests with sanitizers, and checks the formatting.

## License

sqlitexx is distributed under the [Boost Software License, Version 1.0](LICENSE).

It uses [SQLite](https://sqlite.org/copyright.html), which is in the public domain, and
[expected-lite](https://github.com/nonstd-lite/expected-lite), which is also under the Boost Software License.
