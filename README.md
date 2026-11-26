# sqlitexx

A C++ wrapper for [SQLite](https://sqlite.org) in two layers:

- **A typed layer** where everything that can fail returns `expected<T, error>`. Calls chain with `and_then`,
  `or_else` and `transform`, values are bound and read as C++ types, and NULL and out-of-range integers are errors
  rather than silent zeros.
- **A native layer** that mirrors the SQLite C API one to one: RAII handles, result codes, no exceptions, no
  allocations. Use it where the typed layer doesn't reach, or as a thin, safe base for your own abstractions.

> **Status:** early development. The API may still change between commits.

The [API reference](https://lallerhun.github.io/sqlitexx/) documents every class and header of the latest release.

## First steps

Add sqlitexx to your CMake project (see [Using sqlitexx in your project](#using-sqlitexx-in-your-project)), then
include everything at once:

```cpp
#include <sqlitexx/sqlitexx.hpp>

namespace sql = sqlitexx;
```

### 1. Open a database

```cpp
sql::expected<sql::database> opened = sql::database::open(":memory:");
if (!opened)
{
    std::printf("cannot open: %s\n", opened.error().message().c_str());
    return 1;
}
sql::database& db = *opened;
```

Every call that can fail returns an `expected<T, error>`: the value, or the error that stopped it. sqlitexx never
throws. `":memory:"` is a database that lives only as long as `db`; give a file name to keep it on disk.

### 2. Write

```cpp
sql::expected<void> written =
    db.execute("CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, age INTEGER)")
        .and_then(
            [&db]
            {
                return db.execute("INSERT INTO user(name, age) VALUES(?, ?)", "Alice", 31);
            }
        );

// SQL that runs many times: prepare it once. optional<T> binds and reads NULL.
sql::expected<sql::statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(?, ?)");
if (!insert || !insert->execute("Bob", sql::optional<int>()))
{
    ...
}
```

`execute(sql)` runs SQL without values, even several statements at once. `execute(sql, args...)` binds the
arguments to the `?` placeholders in order, so values never need to be pasted into the SQL text. `and_then` runs
the next step only when the previous one succeeded.

### 3. Read

```cpp
struct user
{
    std::string name;
    sql::optional<int> age;
};
SQLITEXX_FIELDS(user, name, age)

for (sql::expected<user>& u : db.query<user>("SELECT name, age FROM user ORDER BY id"))
{
    if (!u)
    {
        std::printf("cannot list: %s\n", u.error().message().c_str());
        break;
    }
    std::printf("%s: %s\n", u->name.c_str(), u->age ? std::to_string(*u->age).c_str() : "unknown");
}

sql::expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM user").first();
```

`SQLITEXX_FIELDS` names the members of your struct that columns go into, once, next to the struct. Each field is
filled from the column with its name, so the order in the `SELECT` doesn't matter. For a single value, or without a
struct, `query<T...>` takes one type per column: the value for one column, a `std::tuple` for several. Each row is
an `expected` too, so an error in the middle of the rows can't slip through. `first()` reads just the first row, and
`all()` reads every row into a `std::vector`. See [Queries](#queries) and [Rows as structs](#rows-as-structs).

### 4. Handle errors

```cpp
sql::expected<void> duplicate = db.execute("INSERT INTO user(name, age) VALUES(?, ?)", "Alice", 32);
if (!duplicate)
{
    std::printf("expected failure: %s\n", duplicate.error().message().c_str());
}

// on() turns one kind of error into a value: no such user gives -1
sql::expected<int> carol = db.query<int>("SELECT age FROM user WHERE name = ?", "Carol")
                               .first()
                               .or_else(
                                   sql::on(
                                       sql::native::primary_code::done,
                                       [](const sql::error&)
                                       {
                                           return sql::expected<int>(-1);
                                       }
                                   )
                               );
```

An `error` holds SQLite's result code and message. Ignoring a returned `expected` is a compiler warning, so an error
can't be dropped by accident. `on(code, handler)` inside `or_else` handles one kind of error and passes the others
on. See [Errors](#errors) for the codes.

The whole program is [examples/quick_start.cpp](examples/quick_start.cpp); CI builds and runs it on every change.
It prints:

```text
Alice: 31
Bob: unknown
users: 2
expected failure: UNIQUE constraint failed: user.name
Carol: -1
```

### Good to know

- **Parameters count from 1, columns from 0.** That is SQLite's own numbering: the first `?` is parameter 1
  (`bind_at(1, value)`), the first column of a row is column 0 (`column<T>(0)`). The calls that take all values at
  once (`execute(sql, args...)`, `query<T...>`) handle the numbering for you.
- **NULL goes only into `optional<T>`.** Reading NULL into an `int` or a `std::string` is an error, not a silent 0
  or empty string. Integers that don't fit their type are errors too.
- **One statement at a time.** `query`, `prepare` and `execute` with arguments reject SQL holding more than one
  statement, rather than running the first and ignoring the rest. `execute(sql)` without arguments runs scripts.
- **Views point into SQLite's memory.** A `string_view` read from a row is valid until the next step. Read a
  `std::string` to keep the text; where a view would outlive its row, sqlitexx refuses to compile.
- **Large tables:** to read page after page, prefer `WHERE id > ? ORDER BY id LIMIT 20` with the last id you saw
  over `OFFSET`, which SQLite still has to step through.

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
find_package(sqlitexx 0.6 REQUIRED) # with <prefix> in CMAKE_PREFIX_PATH
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
too, so one kind of check covers both.

An ignored error is a compiler warning: every call that returns an `expected` is `[[nodiscard]]` from C++17 (and
with Clang from C++11), so `db.execute("DELETE FROM t");` without looking at the result warns. Where ignoring it is
meant, say so: `static_cast<void>(db.execute("DELETE FROM t"));`. The chaining helpers (`first<T>()`,
`execute_with(...)`, ...) are marked too, as one that isn't passed to `and_then` does nothing.

| Primary code | Meaning in sqlitexx |
|---|---|
| `done` | `first()` found no row |
| `mismatch` | NULL read into a non-optional type, or an integer that doesn't fit |
| `nomem` | Out of memory, in SQLite or in sqlitexx (see [Out of memory](#out-of-memory)) |
| `misuse` | `prepare()`, `query()` or `execute(sql, args...)` was given no SQL statement, or more than one |

`sqlitexx::on(code, handler)` handles one kind of error inside `or_else` and passes every other error on unchanged.
`code` is either of these:

- A primary code, such as `native::primary_code::constraint`, which matches every extended code under it.
- An exact extended code, such as `native::extended_code::constraint_unique`.

The extended codes are named after `sqlite3.h` without the prefix (`SQLITE_CONSTRAINT_UNIQUE` becomes
`constraint_unique`), so you don't need `sqlite3.h` for them. `error::code()` compares with both kinds:
`e.code() == native::extended_code::busy_timeout`. For a code that a newer SQLite adds and the enum doesn't list yet,
`on()` also takes the number as an `int`.

```cpp
auto added = db.execute("INSERT INTO user(name) VALUES(?)", "Alice")
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
- `create_function`, `create_aggregate`, `create_collation` and the chaining helpers take what they keep by value.
  Passing an lvalue copies it where you call them, in your code; the calls themselves only move, and are `noexcept`
  when moving can't throw. So nothing they keep refers to your variables.
- sqlitexx catches only its own allocations. What your code throws reaches you unchanged, `std::bad_alloc` included:
  your traits, your callbacks, the copy constructors of what you register. The statement stays usable afterwards.
  The one exception is code that SQLite calls, where nothing may unwind: in SQL functions and aggregates every
  exception becomes an error of the SQL call, and in a collation, which can't fail, it ends the program.
- When an error's message can't be copied, the error keeps its code and goes without a message.

Without exceptions, a failed allocation ends the program, as it does in the standard library. SQLite's own out of
memory is a `nomem` error either way. With MSVC's iterator debugging (its Debug builds), the standard library gives
every `std::string` a proxy object on the heap, even an empty one, and ends the program itself when that fails; the
guarantee holds in release builds.

### Queries

`db.query<T...>(sql, args...)` runs a query with its arguments bound to parameters 1, 2, 3, ... and gives its rows,
one type per column. Iterate it, or take the first row or all of them:

```cpp
for (sql::expected<std::tuple<std::string, int>>& user :
     db.query<std::string, int>("SELECT name, age FROM user WHERE age >= ?", 18))
{
    if (!user)
    {
        std::printf("%s\n", user.error().message().c_str());
        break;
    }
    std::printf("%s is %d\n", std::get<0>(*user).c_str(), std::get<1>(*user));
}

sql::expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM user").first();
sql::expected<std::vector<std::string>> names = db.query<std::string>("SELECT name FROM user").all();

// Pages: next(count) reads on from where the previous page stopped; an empty page means the end
sql::results<std::string> users = db.query<std::string>("SELECT name FROM user ORDER BY name");
sql::expected<std::vector<std::string>> page = users.next(20);
```

Rows 3 to 8 can come from SQL or from C++:

```cpp
// LIMIT and OFFSET: SQLite skips the first rows without reading their columns
sql::expected<std::vector<std::string>> some =
    db.query<std::string>("SELECT name FROM user ORDER BY name LIMIT ? OFFSET ?", 6, 2).all();

// skip(count) passes over rows, then next(count) reads on; skip gives how many it passed
sql::results<std::string> names = db.query<std::string>("SELECT name FROM user ORDER BY name");
sql::expected<std::size_t> skipped = names.skip(2);
sql::expected<std::vector<std::string>> rows_3_to_8 = names.next(6);

// C++20: results is a range, so the standard views work on it too
for (sql::expected<std::string>& name : names | std::views::drop(2) | std::views::take(6)) { ... }
```

- The rows are a `results<T...>`, which owns its statement, so the loop above is safe in every C++ standard.
- Each element is an `expected`. When preparing or binding fails, the error is the only element; when a row can't
  be read (NULL into a non-optional type), only that row is an error; when stepping fails, that error is the last
  element. `first()` and `all()` give the first error they meet.
- `first()` gives a `done` error when there is no row, so `on(native::primary_code::done, ...)` turns that into a
  value. `all()` reads every row into a `std::vector`.
- Every loop, `first()` and `all()` runs the query again from the start and sees the data as it is then. `next()`
  and `skip()` go on from where the previous one stopped, and start over after any of the others. Rows are read one step
  at a time: nothing is cached or read ahead.
- A row that can't be read fails its page in `next()`, and the next page goes on after it.
- Inside a loop, `string_view` and the other views point into the current row. `first()` on a temporary, as in
  `db.query<sql::string_view>(sql).first()`, doesn't compile with them, since the statement ends before you could
  use the view: read a `std::string` or a blob, or keep the `results` in a variable. `all()` and `next()` never take
  views, because each step ends the views of the row before. Your own
  `value_traits` say they read a view with `static constexpr bool reads_view = true;`. The `first<T>()` chaining
  step on a statement fresh from `prepare()` checks the same.

For SQL whose rows you don't need, and for SQL you run many times:

```cpp
sql::expected<void> added = db.execute("INSERT INTO user(name, age) VALUES(?, ?)", "Dee", 41);

sql::expected<sql::statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(?, ?)", "Eve", 29);
```

- `execute(sql)` without arguments runs a script of several statements. With arguments, `execute`, `query` and
  `prepare` take one statement, and SQL holding more is a `misuse` error, as with `prepare(sql)`.
- `query` and `execute` prepare the SQL on every call. For SQL you run many times, prepare it once (with or without
  arguments) and call `statement::execute(args...)`, `first()` or `rows()` on it.

### Rows as structs

```cpp
struct user
{
    std::string name;
    sql::optional<int> age;
    std::int64_t id = 0;
};
SQLITEXX_FIELDS(user, name, age, id)   // namespace scope, in the struct's namespace, no semicolon

sql::expected<user> alice = db.query<user>("SELECT * FROM user WHERE id = ?", 1).first();
```

- Each field is filled from the column with its name, compared like SQL identifiers, so the column order doesn't
  matter and `SELECT *` works. A field without a column is a `range` error that names the column; columns without a
  field are ignored; an alias fits a column to a field: `SELECT years AS age`.
- Fields read like columns of their own type: NULL only into an `optional` field, integers range-checked.
- A struct works wherever a row is read: `query`, `first()`, `all()`, `next()`, `statement::rows<user>()`,
  `first<user>()` and `for_each_row<user>(...)`. It is read on its own, not alongside other types.
- A struct with a `string_view` member counts as a view, with the same compile-time checks.
- The struct must be default-constructible, and must not also have `value_traits`. The macro takes up to 32 members.
- `SQLITEXX_FIELDS` defines a function found by argument-dependent lookup. Write it by hand to read a column of
  another name; from C++14 on that is short:

```cpp
inline auto sqlitexx_fields(const user*)
{
    return sql::fields(sql::field("name", &user::name), sql::field("years", &user::age));
}
```

For what a field list can't fill, specialize `record_traits`, the row-sized counterpart of `value_traits`: a type
without a default constructor, computed or checked fields, or a whole class template through a partial
specialization. `record_builder` reads the columns one after the other, and the first error wins, so there is no
check after each:

```cpp
struct point
{
    point(double x, double y);
    double x, y;
};

template <>
struct sqlitexx::record_traits<point>
{
    static sql::expected<point> read(const sql::statement& row)
    {
        return sql::record_builder(row)
            .column<double>("x")       // by name, or by index: .column<double>(0)
            .column<double>("y")
            .build<point>();           // point(x, y), or point{x, y} for an aggregate
    }
};
```

- `build(factory)` instead calls `factory(values...)`. The factory may return an `expected`, to reject a row with an
  error of its own.
- A record is read wherever a row is read, on its own, and must not also have `value_traits`; the structs that
  `SQLITEXX_FIELDS` names are records through the default `record_traits`.
- A hand-written record whose `read()` keeps views into the row says so with `static constexpr bool reads_view =
  true;`, for the same compile-time checks as a view.

Records bind the other way too: pass one, on its own, wherever values are bound, and its fields go to the named
parameters of the same name.

```cpp
user dee;
dee.name = "Dee";
dee.age = 41;
sql::expected<void> added = db.execute("INSERT INTO user(name, age) VALUES(:name, :age)", dee);
sql::expected<user> found = db.query<user>("SELECT * FROM user WHERE name = :name", dee).first();
```

- Each parameter takes the field named like it without the prefix: `:name`, `@name` and `$name` all take `name`,
  compared exactly, as SQLite compares parameter names. A parameter used twice gets the same value.
- A parameter without a field, or a positional `?`, is a `range` error, so no parameter is left NULL by accident.
  Fields without a parameter are left out, like `id` above.
- `execute`, `query`, `prepare`, `statement::bind` and `statement::execute` all take a record; it can't be mixed with
  other values in one call.
- A hand-written `record_traits` binds with `bind(statement&, const T&)`, and `record_binder` binds the values one
  after the other, the first error winning. `done()` also fails when a parameter got no value:

```cpp
static sql::expected<void> bind(sql::statement& stmt, const point& p)
{
    return sql::record_binder(stmt).bind(":x", p.x).bind(":y", p.y).done();   // by name, or by index from 1
}
```

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
  SQLite keeps a copy until the function is replaced or removed, or the database closes.
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

### Collations

`database::create_collation(name, compare)` gives SQLite a sort order written in C++, for `ORDER BY`, comparisons
and indexes. `compare` takes two `string_view`s and returns a negative number, zero or a positive number, like
`compare()` on strings:

```cpp
db.create_collation("by_length", [](sql::string_view a, sql::string_view b) {
    return a.size() != b.size() ? (a.size() < b.size() ? -1 : 1) : a.compare(b);
});

db.prepare("SELECT name FROM fruit ORDER BY name COLLATE by_length");
db.execute("CREATE TABLE tag(name TEXT COLLATE by_length UNIQUE)");   // UNIQUE follows the collation too
```

- In C++20, `compare` may return what `operator<=>` gives, `std::strong_ordering` or `std::weak_ordering`:
  `[](sql::string_view a, sql::string_view b) { return a <=> b; }`.
- A `bool` result doesn't compile: a less-than predicate can't tell SQLite that two texts are equal.
- The texts arrive as UTF-8 and are not NUL-terminated.
- The order must be consistent: the same texts always compare the same way. SQLite relies on it for sorting and
  indexes, so an index built with an inconsistent collation, or with one that changed since, is corrupt.
- SQLite can't take an error from a comparison. With exceptions enabled, an exception from `compare` ends the program
  (`std::terminate`), since it may not unwind through SQLite and an unknown order could corrupt an index.
- SQLite keeps `compare` until the collation is replaced or removed, or the database closes. Registering the same
  name again replaces it, which fails with `busy` while statements are running.

### Functions and collations with shared state

Functions, aggregates and collations can refer to state outside them, shared between them, as long as SQLite stops
calling them before the state ends. A `registrations` token takes care of that: what is registered with it is removed
when the token ends, on every way out of the scope.

```cpp
struct sort_settings { bool descending = false; } settings;
sql::registrations scope;   // declared after the state, so it ends first

db.create_collation("configured", [&settings](sql::string_view a, sql::string_view b) {
    return settings.descending ? b.compare(a) : a.compare(b);
}, scope);
db.create_function("set_descending", [&settings](bool descending) { settings.descending = descending; }, scope);

// ... SELECT set_descending(1); SELECT name FROM fruit ORDER BY name COLLATE configured ...
// the end of the scope removes both, then settings ends
```

- Every `create_function`, `create_aggregate` and `create_collation` takes a token as its last parameter, and so does
  every [hook](#hooks).
- `scope.remove()` removes now and returns the first error; `scope.release()` keeps the registrations and forgets them.
- What is gone already needs nothing: replaced, removed by name, or released because the database closed. The token
  may outlive the database, and may hold registrations of several databases.
- Removing fails with `busy` while any statement on the connection is running: it returned a row and wasn't reset
  or finished. Statements declared after the token end before it. When the token can't remove a registration, it
  ends the callable instead: a function or aggregate called afterwards fails with `misuse`, and a collation ends the
  program (`std::terminate`), since it can't fail and comparing any other way could corrupt an index. Calling
  `scope.remove()` and checking its result avoids that; so does creating the token outside a loop over rows.

Without a token, `remove_function(name, argument_count)` and `remove_collation(name)` take back a registration by
name. SQLite releases its copy at once:

```cpp
db.remove_collation("configured");
db.remove_function("set_descending", 1);   // a function is known by its name and number of parameters
```

- For an aggregate, the number of parameters is that of `step()`.
- Removing fails with `busy` while statements are running, and then nothing is released.
- Removing what isn't registered succeeds and does nothing.
- Statements prepared earlier are prepared again before their next step, so those using the removed name fail then.

For state that should simply live as long as SQLite needs it, capture a `std::shared_ptr` instead: no token or
removing needed.

### Hooks

Hooks run C++ code when transactions commit or roll back, when rows change, and while statements run:

```cpp
std::vector<std::int64_t> changed;
std::atomic<bool> cancelled{false};   // set from another thread to cancel
sql::registrations scope;

db.update_hook([&changed](sql::native::update_kind kind, sql::cstring_view schema, sql::cstring_view table,
                          std::int64_t rowid) { changed.push_back(rowid); }, scope);
db.commit_hook([&] { return changed.size() < 1000; }, scope);   // false turns the commit into a rollback
db.rollback_hook([&] { changed.clear(); }, scope);
db.progress_handler(1000, [&] { return !cancelled; }, scope);    // every 1000 VM instructions; false interrupts
```

- `commit_hook`: before every commit, also the automatic one around a single statement. It returns `void`, or a
  `bool`: false turns the commit into a rollback, and the statement fails with `extended_code::constraint_commithook`.
- `rollback_hook`: after a rollback, by `ROLLBACK` or by an error.
- `update_hook`: for every row inserted, updated or deleted in a rowid table, with the kind of change
  (`update_kind::insert`, `update` or `remove`), the schema, the table and the rowid.
- `progress_handler(instructions, handler)`: while a statement runs; false interrupts it, and it fails with
  `interrupt`. Useful for cancelling long queries.
- A connection has one hook of each kind. Setting one replaces the one before, and `nullptr` clears it:
  `db.commit_hook(nullptr)`. With a `registrations` token, it is cleared when the token ends; that always works, also
  while statements run.
- A hook must not change the connection running it: no statements on it from inside a hook.
- Hooks can't report errors. With exceptions enabled, an exception from a commit hook rolls back and one from a
  progress handler interrupts; one from an update or rollback hook ends the program (`std::terminate`).

### Rows as a range

`statement::rows<T...>()` gives the rows of a prepared statement for a range-based `for` loop, like
[`query()`](#queries) does for SQL. Each element is an `expected<row<T...>>`: the value for one column, a
`std::tuple` for several, or the error.

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
