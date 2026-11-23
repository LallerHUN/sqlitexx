// Uses an installed sqlitexx through find_package: the headers, the library, the bundled or system SQLite and
// expected all have to arrive through the sqlitexx::sqlitexx target.

#include <sqlitexx/sqlitexx.hpp>

#include <cstdio>

int main()
{
    namespace sql = sqlitexx;

    sql::expected<sql::database> db = sql::database::open(":memory:");
    if (!db)
    {
        std::printf("open failed: %s\n", db.error().message().c_str());
        return 1;
    }

    sql::expected<int> answer = sql::in_transaction(
        *db,
        [&]
        {
            return db->execute("CREATE TABLE t(v INTEGER); INSERT INTO t VALUES(42)")
                .and_then(
                    [&]
                    {
                        return db->prepare("SELECT v FROM t").and_then(sql::first<int>());
                    }
                );
        }
    );

    if (!answer || *answer != 42)
    {
        std::printf("query failed\n");
        return 1;
    }
    std::printf("installed sqlitexx works: %d\n", *answer);
    return 0;
}
