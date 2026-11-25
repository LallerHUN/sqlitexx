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

- A C++11 compiler or newer. Tested in CI with MSVC (Visual Studio 2026, C++14 and newer), GCC and Clang on Linux
  (C++11 to C++23), and AppleClang on macOS.
- CMake 3.21 or newer.
- SQLite 3.37 or newer. By default CMake downloads and builds SQLite itself, so nothing needs to be installed.

## Using sqlitexx in your project

Installed, through its CMake package:

```shell
cmake -S . -B build
cmake --build build --config Release
cmake --install build --config Release --prefix <prefix>
```

```cmake
find_package(sqlitexx 0.2 REQUIRED) # with <prefix> in CMAKE_PREFIX_PATH
target_link_libraries(my_app PRIVATE sqlitexx::sqlitexx)
```

The bundled SQLite and backports (expected-lite, and before C++17 string-view-lite and optional-lite) are installed
with it. When sqlitexx was built with one of the `SQLITEXX_USE_SYSTEM_*` options, the package finds those again in
your build.
Debug libraries carry a `d` suffix, so Debug and Release can be installed into the same prefix.

Or built along with your project, with CMake's `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(sqlitexx
    GIT_REPOSITORY https://github.com/LallerHUN/sqlitexx.git
    GIT_TAG main # the latest release; pin a version tag for reproducible builds
)
FetchContent_MakeAvailable(sqlitexx)

target_link_libraries(my_app PRIVATE sqlitexx::sqlitexx)
```

Or as a subdirectory: `add_subdirectory(sqlitexx)`.

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
| `SQLITEXX_CXX17_TYPES` | `AUTO` | Which `string_view` and `optional` the API uses: `STD` (C++17's), `LITE` ([string-view-lite](https://github.com/nonstd-lite/string-view-lite) and [optional-lite](https://github.com/nonstd-lite/optional-lite)), or `AUTO`, which picks the backports when `CMAKE_CXX_STANDARD` is below 17. |
| `SQLITEXX_USE_SYSTEM_LITE_TYPES` | `OFF` | Use an installed string-view-lite and optional-lite instead of downloading the pinned ones. |
| `SQLITEXX_BUILD_TESTS` | top-level only | Build the tests. |
| `SQLITEXX_BUILD_EXAMPLES` | top-level only | Build the examples and run them as tests. |
| `SQLITEXX_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings in sqlitexx and its tests as errors. CI turns it on. |
| `SQLITEXX_INSTALL` | top-level only | Generate the install rules and the CMake package. |

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
| `cstring_view` | as text | like `string_view`, and always followed by a `'\0'`, so `c_str()` can go to C functions |
| `const char*` | as text, or NULL for `nullptr` | not readable |
| `nullptr` | NULL | not readable |
| `sqlitexx::blob` (`std::vector<unsigned char>`) | as a blob | |
| `std::vector<std::byte>` (C++17) | as a blob | |
| `sqlitexx::zeroblob{size}` | as a blob of `size` zero bytes, without building it in memory | not readable |
| `std::span<const std::byte>`, `std::span<const unsigned char>` (C++20) | as a blob, without copying it first | a view into SQLite's memory until the next step |
| `std::span` of writable bytes, or with a fixed extent (C++20) | as a blob | not readable |
| `std::u8string`, `std::u8string_view`, `u8cstring_view` (C++20) | as UTF-8 text | like their `char` counterparts |
| `const char8_t*`, such as `u8"..."` (C++20) | as UTF-8 text, or NULL for `nullptr` | not readable |
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
| `nomem` | Out of memory, in SQLite or in sqlitexx (see [Out of memory](#out-of-memory)) |
| `misuse` | `prepare()` was given no SQL statement, or more than one |

`sqlitexx::on(code, handler)` handles one kind of error inside `or_else` and passes every other error on unchanged.
`code` is either of these:

- A primary code, such as `native::primary_code::constraint`, which matches every extended code under it.
- An exact extended code, such as `native::extended_code::constraint_unique`.

The extended codes are named after `sqlite3.h` without the prefix (`SQLITE_CONSTRAINT_UNIQUE` becomes
`constraint_unique`), so you don't need `sqlite3.h` for them. `error::code()` compares with both kinds:
`e.code() == native::extended_code::busy_timeout`. For a code that a newer SQLite adds and the enum doesn't list yet,
`on()` also takes the number as an `int`.

```cpp
auto added = db.prepare("INSERT INTO user(name) VALUES(?)")
                 .and_then(sql::execute_with("Alice"))
                 .transform([] { return true; })
                 .or_else(sql::on(sql::native::extended_code::constraint_unique, [](const sql::error&) {
                     return sql::expected<bool>(false); // already there
                 }));
```

### Out of memory

With exceptions enabled, no sqlitexx call lets `std::bad_alloc` out. Running out of memory, whether in SQLite or in
sqlitexx copying text, blobs or error messages, is an error with `primary_code::nomem`:

```cpp
sql::expected<std::string> name = stmt->column<std::string>(0);   // noexcept
if (!name && name.error().primary() == sql::native::primary_code::nomem)
{
    // no try/catch needed
}
```

- The calls that run none of your code are `noexcept`: `database::open`, `execute`, `prepare`, `backup_to`,
  `open_blob`, `statement::step` and `reset`, the `blob_stream` calls that take a pointer, and `transaction`.
- The calls that bind or read values are `noexcept` when the `value_traits` of the types are. The built-in ones are,
  also for `std::string` and `blob`, so `bind(1, name)` and `column<std::string>(0)` are `noexcept`. With your own
  traits they follow your `bind()` and `read()`: declare them `noexcept` when they cannot throw.
- `create_function`, `create_aggregate` and the chaining helpers take what they keep by value. Passing an lvalue
  copies it where you call them, in your code; the calls themselves only move, and are `noexcept` when moving can't
  throw. So nothing they keep refers to your variables.
- sqlitexx catches only its own allocations. What your code throws reaches you unchanged, `std::bad_alloc` included:
  your traits, your callbacks, the copy constructors of what you register. The statement stays usable afterwards.
  The one exception is code that SQLite calls, SQL functions and aggregates: there every exception becomes an error
  of the SQL call, since none may unwind through SQLite.
- When an error's message can't be copied, the error keeps its code and goes without a message.

Without exceptions, a failed allocation ends the program, as it does in the standard library. SQLite's own out of
memory is a `nomem` error either way. With MSVC's iterator debugging (its Debug builds), the standard library gives
every `std::string` a proxy object on the heap, even an empty one, and ends the program itself when that fails; the
guarantee holds in release builds.

### Chaining helpers

| Helper | Use with | Does |
|---|---|---|
| `bind_values(args...)` | `and_then` | binds `args` to parameters 1, 2, 3, ... |
| `first<T...>()` | `and_then` | steps once and reads the row |
| `for_each_row<T...>(callback)` | `and_then` | calls `callback` with every row |
| `execute_with(args...)` | `and_then` | binds `args` and runs the statement to completion |
| `on(code, handler)` | `or_else` | handles errors with that code |

### Columns by name

`statement::column<T>(index)` reads one column of the current row. `column<T>(name)` reads it by name, and
`column_index(name)` gives the index:

```cpp
sql::expected<sql::statement> select = db.prepare("SELECT name, count(*) AS orders FROM purchase GROUP BY name");
while (select && select->step().value_or(false))
{
    sql::expected<std::string> name = select->column<std::string>("name");
    sql::expected<int> orders = select->column<int>("orders");
}
```

- A column's name is its `AS` alias, or else what SQLite makes of the expression, so give computed columns an alias.
- Names compare like SQL identifiers: `"name"` finds a column called `Name`. With a repeated name, the first column
  wins.
- No column by that name is a `range` error.
- Each call searches the columns. In a loop over many rows, call `column_index()` once and read by index.

### SQL functions

`database::create_function(name, callable)` makes a C++ function callable from SQL. The parameters and the return
type are read and written like columns and parameters, so every type in the Values table works:

```cpp
db.create_function("greet", [](const std::string& name) { return "Hello, " + name; });

sql::expected<std::string> hello = db.prepare("SELECT greet('Alice')").and_then(sql::first<std::string>());
```

- The callable is a lambda without `auto` parameters, a function object with one `operator()`, or a function pointer.
  SQLite keeps a copy until the function is replaced or the database closes.
- NULL arrives only in an `optional<T>` parameter; anywhere else the call fails with `mismatch`. Returning an empty
  `optional<T>`, or returning `void`, gives NULL.
- To fail the call, return an `expected<T>` holding an error: the statement fails with its code and message. With
  exceptions enabled, a thrown exception fails the call too, and never reaches SQLite.
- Pass `native::function_flags::deterministic` when the same arguments always give the same result, so SQLite can
  optimize the calls and use the function in an index.

Your own types work as arguments and results when their `value_traits` are templates, as in
[sqlitexx/value.hpp](include/sqlitexx/value.hpp).

### Aggregate and window functions

`database::create_aggregate(name, prototype)` makes a class an aggregate, used like `SUM()` or `GROUP_CONCAT()`.
The class has a `step(...)` member, called for every row, and a `finish()` member, which gives the result:

```cpp
struct joined
{
    std::string separator;
    std::string text;

    void step(const std::string& value) { text += text.empty() ? value : separator + value; }
    std::string finish() const { return text; }
};

db.create_aggregate("joined", joined{", ", ""});

sql::expected<std::string> names = db.prepare("SELECT joined(name) FROM player WHERE team = 'red'")
                                       .and_then(sql::first<std::string>());   // "Ann, Bob, Dee"
```

- Each group works on its own copy of the prototype, so the prototype can carry settings like the separator.
- `step()` takes its parameters like an SQL function: NULL only into `optional<T>`. It returns `void`, or
  `expected<void>` to fail the query with that error.
- `finish()` returns the result like an SQL function does: `optional<T>` for NULL, `expected<T>` to fail. For an
  empty table it runs on a fresh copy that saw no rows, so a count gives 0.

A class that also has `inverse(...)` (taking a row back out) and `value()` (the result so far) is registered as a
window function too, for `OVER (...)`:

```cpp
struct moving_sum
{
    std::int64_t sum = 0;

    void step(std::int64_t value) { sum += value; }
    void inverse(std::int64_t value) { sum -= value; }
    std::int64_t value() const { return sum; }
    std::int64_t finish() const { return sum; }
};

db.create_aggregate("moving_sum", moving_sum());
// SELECT moving_sum(amount) OVER (ORDER BY day ROWS BETWEEN 6 PRECEDING AND CURRENT ROW) FROM sales
```

Without `inverse()` and `value()`, using the aggregate with `OVER` is an error when the statement is prepared.

### Rows as a range

`statement::rows<T...>()` gives the rows of a query for a range-based `for` loop. Each element is an
`expected<row<T...>>`: the value for one column, a `std::tuple` for several, or the error.

```cpp
for (sql::expected<std::tuple<std::string, int>>& user : select.rows<std::string, int>())
{
    if (!user)
    {
        std::printf("%s\n", user.error().message().c_str());
        break;
    }
    std::printf("%s is %d\n", std::get<0>(*user).c_str(), std::get<1>(*user));
}
```

- A row that can't be read (NULL into a non-optional type, an integer that doesn't fit) is an error for that row
  only; the loop may go on. When stepping fails, that error is the last element.
- Iteration starts where the statement is, without a reset, like `first()` and `for_each()`.
- It is a single-pass input range. The statement must outlive it, so call `rows()` on a named statement, not on the
  result of `db.prepare(...)`. In C++20 it is a `std::ranges::input_range`, so `std::views::take`, `filter` and
  `transform` work on it.

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

### Backup

`source.backup_to(destination)` copies a whole database over another one, while both stay usable. It's how an
in-memory database gets saved to a file, and restoring is the same call the other way round:

```cpp
sql::expected<sql::database> file = sql::database::open("saved.sqlite");
sql::expected<void> saved = file ? memory.backup_to(*file) : sql::expected<void>(sql::unexpect, file.error());
```

The call belongs to the database being copied, so the direction is clear: `a.backup_to(b)` replaces everything in
`b` with `a`'s content.

For large databases, `begin_backup_to(destination)` gives a `backup` that copies in steps: `step(pages)` returns
true once the copy is complete, and `remaining()` and `page_count()` report progress. Between steps, other
connections can use the source. A `busy` or `locked` error from `step()` means a lock was in the way, and stepping
again later retries.

### Large blobs in pieces

`open_blob(table, column, rowid)` opens one blob for reading or writing in pieces, so a large one never has to fit in
memory whole. The `blob_stream` it gives has a position, like a file:

```cpp
// Reserve the space first: a blob's size can't change through a stream
db.prepare("INSERT INTO file(data) VALUES(?)").and_then(sql::execute_with(sql::zeroblob{size}));
sql::expected<sql::blob_stream> out =
    db.open_blob("file", "data", db.last_insert_rowid(), sql::blob_access::read_write);
out->write(chunk);                        // continues from the position each time

sql::expected<sql::blob_stream> in = db.open_blob("file", "data", rowid);
std::vector<unsigned char> all;
in->read(all);                            // the rest of the blob; all.size() is what was read
sql::expected<std::size_t> got = in->read(all, 4096);   // at most 4096 bytes; *got == all.size()

std::array<unsigned char, 4096> buffer;   // or in pieces, without allocating
sql::expected<std::size_t> filled = in->read(buffer);   // up to buffer.size(); 0 at the end
```

- `write()` takes any contiguous container of bytes (`std::vector`, `std::string`, `std::array`, `std::span`,
  `string_view`, C arrays), or a pointer and a size. `write(data, count)` writes only the first `count` bytes.
- `read()` into a container that can grow (`std::vector`, `std::string`) sizes it to what was read. Into one that
  can't (`std::array`, `std::span`, C arrays) it fills up to the size, without allocating. `read(out, count)` reads
  at most `count` bytes.
- A `count` larger than the container is a `range` error, before anything is read or written.
- Containers hold bytes: `char`, `signed char`, `unsigned char`, `std::byte` or `char8_t`. Wider elements don't
  compile, since their bytes depend on the machine.
- `read()` gives how many bytes it read: fewer near the end, 0 at the end. `seek()` moves the position.
- Writing past the end is a `range` error and writes nothing. Reserve the size with `zeroblob` when inserting.
- `reopen(rowid)` moves to the same column of another row, much faster than opening again.
- When the row changes through another statement, the stream expires and reads and writes fail with `abort`.

### Waiting for locks

When another connection holds a lock, a statement fails with `primary_code::busy` at once. `busy_timeout()` makes it
retry for a while first:

```cpp
db.busy_timeout(std::chrono::seconds(5));
```

Zero turns waiting off again, as it is when a database opens.

### Standard types or backports

`sqlitexx::expected` is `std::expected` or expected-lite's, depending on `SQLITEXX_EXPECTED`, and
`sqlitexx::string_view` and `sqlitexx::optional` are the C++17 ones or string-view-lite's and optional-lite's,
depending on `SQLITEXX_CXX17_TYPES`. CMake decides once and passes the choice to everything that links sqlitexx, so
the library and your code always agree on which types the API uses. Write `sqlitexx::expected`,
`sqlitexx::unexpected`, `sqlitexx::unexpect`, `sqlitexx::string_view`, `sqlitexx::optional` and `sqlitexx::nullopt`,
and your code works with either.

The backports convert from and to `std::string` like the standard types do, but not from or to `std::string_view` and
`std::optional`: in a C++17 program built with `SQLITEXX_CXX17_TYPES=LITE`, convert explicitly.

## The native layer

`sqlitexx::native::connection` and `sqlitexx::native::statement` own a `sqlite3*` and a `sqlite3_stmt*`. Their
functions keep the C names (`prepare`, `step`, `bind_int64`, `column_text`, ...), return a `native::result`, and
never throw. Differences from the C API:

- Connections close with `sqlite3_close_v2`, so statements may outlive their connection.
- Extended result codes are always on.
- Text goes in as `string_view` and blobs as pointer plus size; an empty `string_view` binds an empty text, not NULL.
- Text comes out as `optional<cstring_view>`: `nullopt` where SQLite returns a NULL pointer (a NULL value, or out of
  memory), and otherwise a view that is always followed by a `'\0'`, so an empty string is never mistaken for NULL.
- The public headers don't include `sqlite3.h`.

## Contributing

Contributions are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) covers building and testing, the code rules that
clang-format can't check, and how pull requests get in.

## License

sqlitexx is distributed under the [Boost Software License, Version 1.0](LICENSE).

It uses [SQLite](https://sqlite.org/copyright.html), which is in the public domain, and
[expected-lite](https://github.com/nonstd-lite/expected-lite),
[string-view-lite](https://github.com/nonstd-lite/string-view-lite) and
[optional-lite](https://github.com/nonstd-lite/optional-lite), which are also under the Boost Software License.
