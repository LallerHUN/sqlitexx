// The example from README.md. CI builds and runs it, so the README cannot drift from the API unnoticed.

#include <sqlitexx/sqlitexx.hpp>

#include <cstdio>
#include <string>
#include <tuple>

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

    // Query: one type per column, a std::tuple for several. Each row is an expected, so errors can't slip through.
    bool listed = true;
    for (sql::expected<std::tuple<std::string, sql::optional<int>>>& user :
         db.query<std::string, sql::optional<int>>("SELECT name, age FROM user WHERE id >= ? ORDER BY id", 1))
    {
        if (!user)
        {
            std::printf("cannot list: %s\n", user.error().message().c_str());
            listed = false;
            break;
        }
        const sql::optional<int>& age = std::get<1>(*user);
        std::printf("%s: %s\n", std::get<0>(*user).c_str(), age ? std::to_string(*age).c_str() : "unknown");
    }

    // One row, and one kind of error turned into a value: no such user gives -1
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
    return listed && carol && *carol == -1 ? 0 : 1;
}
