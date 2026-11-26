// Tests for the statement cache behind query() and execute(sql, args...). The connection's open statements are
// counted through the C API, which is why this test, unlike the others, includes sqlite3.h.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::native::primary_code;

namespace
{
database open_numbers()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    CHECK(db->execute("CREATE TABLE number(n INTEGER)").has_value());
    return std::move(*db);
}

// The statements prepared on the connection that haven't been finalized: cached, held by a results, or prepared
int open_statements(const database& db)
{
    int count = 0;
    for (sqlite3_stmt* stmt = sqlite3_next_stmt(db.native_connection().handle(), nullptr); stmt != nullptr;
         stmt = sqlite3_next_stmt(db.native_connection().handle(), stmt))
    {
        ++count;
    }
    return count;
}

std::int64_t count_numbers(database& db)
{
    expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM number").first();
    CHECK(count.has_value());
    return count ? *count : -1;
}

void test_reuse()
{
    database db = open_numbers();
    CHECK(db.statement_cache_size() == 16);

    // The same SQL a hundred times: one statement
    for (int n = 0; n < 100; ++n)
    {
        CHECK(db.execute("INSERT INTO number(n) VALUES(?)", n).has_value());
    }
    CHECK(open_statements(db) == 1);

    // query keeps its statement too, and gives it back when the results end
    CHECK(count_numbers(db) == 100);
    CHECK(count_numbers(db) == 100);
    CHECK(open_statements(db) == 2);

    // prepare() is never cached
    {
        expected<sqlitexx::statement> own = db.prepare("SELECT count(*) FROM number");
        CHECK(own.has_value());
        CHECK(open_statements(db) == 3);
    }
    CHECK(open_statements(db) == 2);
}

void test_held_results()
{
    database db = open_numbers();
    CHECK(db.execute("INSERT INTO number(n) VALUES(1), (2), (3)").has_value());

    // Two results of the same SQL at once: the second prepares its own, and both are kept afterwards
    {
        sqlitexx::results<int> first = db.query<int>("SELECT n FROM number ORDER BY n");
        sqlitexx::results<int> second = db.query<int>("SELECT n FROM number ORDER BY n");
        CHECK(open_statements(db) == 2);
        expected<std::vector<int>> a = first.all();
        expected<std::vector<int>> b = second.all();
        CHECK(a && b && *a == *b && a->size() == 3);
    }
    CHECK(open_statements(db) == 2);

    // Five at once: two copies are kept, and the other SQL stays cached
    CHECK(count_numbers(db) == 3);
    {
        std::vector<sqlitexx::results<int>> many;
        for (int i = 0; i < 5; ++i)
        {
            many.push_back(db.query<int>("SELECT n FROM number ORDER BY n"));
        }
        CHECK(open_statements(db) == 6);
    }
    CHECK(open_statements(db) == 3);
    CHECK(count_numbers(db) == 3);
    CHECK(open_statements(db) == 3);

    // A moved results gives its statement back once
    {
        sqlitexx::results<int> moved = db.query<int>("SELECT n FROM number ORDER BY n");
        sqlitexx::results<int> target = std::move(moved);
        expected<int> one = target.first();
        CHECK(one && *one == 1);
    }
    CHECK(open_statements(db) == 3);
}

void test_size()
{
    database db = open_numbers();

    // Full: the statement used longest ago goes
    db.statement_cache_size(2);
    CHECK(db.execute("INSERT INTO number(n) VALUES(?)", 1).has_value());
    CHECK(db.execute("INSERT INTO number(n) VALUES(? + 1)", 1).has_value());
    CHECK(db.execute("INSERT INTO number(n) VALUES(? + 2)", 1).has_value());
    CHECK(open_statements(db) == 2);

    // Smaller: the oldest go at once; zero turns the cache off
    db.statement_cache_size(1);
    CHECK(open_statements(db) == 1);
    db.statement_cache_size(0);
    CHECK(open_statements(db) == 0);
    CHECK(db.execute("INSERT INTO number(n) VALUES(?)", 4).has_value());
    CHECK(count_numbers(db) == 4);
    CHECK(open_statements(db) == 0);
}

void test_returned_clean()
{
    database db = open_numbers();

    // A statement goes back reset, with nothing bound
    expected<std::string> text = db.query<std::string>("SELECT ?", std::string(1000, 'x')).first();
    CHECK(text && text->size() == 1000);
    sqlite3_stmt* cached = sqlite3_next_stmt(db.native_connection().handle(), nullptr);
    CHECK(cached != nullptr);
    if (cached != nullptr)
    {
        CHECK(sqlite3_stmt_busy(cached) == 0);
        char* expanded = sqlite3_expanded_sql(cached);
        CHECK(expanded != nullptr && std::strcmp(expanded, "SELECT NULL") == 0);
        sqlite3_free(expanded);
    }

    // A statement whose binding fails goes back too
    expected<int> extra = db.query<int>("SELECT ?", 1, 2).first();
    CHECK(!extra && extra.error().primary() == primary_code::range);
    CHECK(open_statements(db) == 1);

    // A failed run doesn't stay with the statement
    CHECK(db.execute("CREATE TABLE once(n INTEGER UNIQUE)").has_value());
    CHECK(db.execute("INSERT INTO once(n) VALUES(?)", 1).has_value());
    expected<void> twice = db.execute("INSERT INTO once(n) VALUES(?)", 1);
    CHECK(!twice && twice.error().primary() == primary_code::constraint);
    CHECK(db.execute("INSERT INTO once(n) VALUES(?)", 2).has_value());
}

void test_lifetime()
{
    // A results that outlives its database still reads, and then finalizes its statement
    sqlitexx::results<int> survivor = []
    {
        database db = open_numbers();
        CHECK(db.execute("INSERT INTO number(n) VALUES(7)").has_value());
        return db.query<int>("SELECT n FROM number");
    }();
    expected<int> seven = survivor.first();
    CHECK(seven && *seven == 7);

    // Moving a database moves its cache; moving one over another finalizes the old cache first
    database a = open_numbers();
    CHECK(a.execute("INSERT INTO number(n) VALUES(?)", 1).has_value());
    database b = std::move(a);
    CHECK(open_statements(b) == 1);
    CHECK(b.execute("INSERT INTO number(n) VALUES(?)", 2).has_value());
    CHECK(open_statements(b) == 1);
    b = open_numbers();
    CHECK(open_statements(b) == 0);
    CHECK(count_numbers(b) == 0);
}

void test_schema_changes()
{
    database db = open_numbers();
    CHECK(db.execute("INSERT INTO number(n) VALUES(?)", 1).has_value());

    // A cached statement prepares itself again after the schema changes
    CHECK(db.execute("ALTER TABLE number ADD COLUMN label TEXT").has_value());
    CHECK(db.execute("INSERT INTO number(n) VALUES(?)", 2).has_value());
    expected<std::int64_t> labels = db.query<std::int64_t>("SELECT count(label) FROM number").first();
    CHECK(labels && *labels == 0);
    CHECK(count_numbers(db) == 2);
}
} // anonymous

int main()
{
    test_reuse();
    test_held_results();
    test_size();
    test_returned_clean();
    test_lifetime();
    test_schema_changes();

    return test::summary();
}
