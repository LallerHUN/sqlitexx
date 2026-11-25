// The program that README.md's "First steps" walks through. CI builds and runs it, so the README cannot drift from the
// API unnoticed: keep the two in step.

#include <sqlitexx/sqlitexx.hpp>

#include <cstdint>
#include <cstdio>
#include <string>

namespace sql = sqlitexx;

// The rows of the user table; SQLITEXX_FIELDS names the members that columns go into
struct user
{
    std::string name;
    sql::optional<int> age;
};
// clang-format off
SQLITEXX_FIELDS(user, name, age)
// clang-format on

int main()
{
    // 1. Open: every call that can fail returns expected<T, error>
    sql::expected<sql::database> opened = sql::database::open(":memory:");
    if (!opened)
    {
        std::printf("cannot open: %s\n", opened.error().message().c_str());
        return 1;
    }
    sql::database& db = *opened;

    // 2. Write: execute(sql) for SQL without values, execute(sql, args...) binds the values to the ? in order
    sql::expected<void> written =
        db.execute("CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, age INTEGER)")
            .and_then(
                [&db]
                {
                    return db.execute("INSERT INTO user(name, age) VALUES(?, ?)", "Alice", 31);
                }
            );
    if (!written)
    {
        std::printf("cannot write: %s\n", written.error().message().c_str());
        return 1;
    }

    // SQL that runs many times: prepare it once. optional<T> binds and reads NULL.
    sql::expected<sql::statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(?, ?)");
    if (!insert || !insert->execute("Bob", sql::optional<int>()))
    {
        std::printf("cannot insert\n");
        return 1;
    }

    // 3. Read: rows into a struct, each field from the column with its name. Each row is an expected, so errors can't
    // slip through.
    bool listed = true;
    for (sql::expected<user>& u : db.query<user>("SELECT name, age FROM user ORDER BY id"))
    {
        if (!u)
        {
            std::printf("cannot list: %s\n", u.error().message().c_str());
            listed = false;
            break;
        }
        std::printf("%s: %s\n", u->name.c_str(), u->age ? std::to_string(*u->age).c_str() : "unknown");
    }

    // One row: first()
    sql::expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM user").first();
    std::printf("users: %lld\n", count ? static_cast<long long>(*count) : -1LL);

    // 4. Errors carry SQLite's code and message
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
    std::printf("Carol: %d\n", carol.value_or(-2));

    return listed && count && *count == 2 && !duplicate && carol && *carol == -1 ? 0 : 1;
}
