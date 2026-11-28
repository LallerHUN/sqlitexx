// Tests for database::create_aggregate: aggregate and window functions written in C++.

#include "check.hpp"
#include "sqlnx/sqlite.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#include <stdexcept>
#define SQLNX_TEST_EXCEPTIONS 1
#else
#define SQLNX_TEST_EXCEPTIONS 0
#endif

using sqlnx::sqlite::database;
using sqlnx::sqlite::error;
using sqlnx::sqlite::expected;
using sqlnx::sqlite::optional;
using sqlnx::sqlite::unexpect;
using sqlnx::sqlite::native::extended_code;
using sqlnx::sqlite::native::primary_code;

namespace
{
// GROUP_CONCAT with a separator chosen when registering
struct joined
{
    std::string separator;
    std::string text;

    void step(const std::string& value)
    {
        text += text.empty() ? value : separator + value;
    }

    std::string finish() const
    {
        return text;
    }
};

// COUNT of the non-NULL values: NULL arrives in the optional
struct counted
{
    std::int64_t count = 0;

    void step(optional<std::int64_t> value)
    {
        if (value)
        {
            ++count;
        }
    }

    std::int64_t finish() const
    {
        return count;
    }
};

// SUM that refuses negative values, and refuses to finish above a limit
struct checked_sum
{
    std::int64_t limit = 0;
    std::int64_t sum = 0;

    expected<void> step(std::int64_t value)
    {
        if (value < 0)
        {
            return expected<void>(
                unexpect,
                error(
                    sqlnx::sqlite::native::result(static_cast<int>(extended_code::constraint_check)),
                    "checked_sum: negative value"
                )
            );
        }
        sum += value;
        return {};
    }

    expected<std::int64_t> finish() const
    {
        if (sum > limit)
        {
            return expected<std::int64_t>(
                unexpect,
                error(
                    sqlnx::sqlite::native::result(static_cast<int>(primary_code::range)),
                    "checked_sum: over the limit"
                )
            );
        }
        return sum;
    }
};

// A moving sum: a window function too
struct moving_sum
{
    std::int64_t sum = 0;

    void step(std::int64_t value)
    {
        sum += value;
    }

    void inverse(std::int64_t value)
    {
        sum -= value;
    }

    std::int64_t value() const
    {
        return sum;
    }

    std::int64_t finish() const
    {
        return sum;
    }
};

// Counts how many copies are alive, through the shared pointer every copy holds
struct tracked
{
    std::shared_ptr<int> token;

    void step(int)
    {
    }

    int finish() const
    {
        return 0;
    }
};

#if SQLNX_TEST_EXCEPTIONS
struct throwing
{
    void step(int value)
    {
        if (value == 2)
        {
            throw std::runtime_error("throwing: no twos");
        }
    }

    int finish() const
    {
        return 0;
    }
};
#endif

database open_scores()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    CHECK(db->execute(
                "CREATE TABLE score(team TEXT, name TEXT, points INTEGER);"
                "INSERT INTO score VALUES('red', 'Ann', 3), ('red', 'Bob', NULL), ('blue', 'Cid', 5),"
                " ('red', 'Dee', 1), ('blue', 'Eve', 2);"
    )
              .has_value());
    return std::move(*db);
}

template <class T>
expected<T> select(
    database& db,
    const char* sql
)
{
    return db.prepare(sql).and_then(sqlnx::sqlite::first<T>());
}

void test_groups_and_prototype()
{
    database db = open_scores();
    joined prototype;
    prototype.separator = ", ";
    CHECK(db.create_aggregate("joined", prototype).has_value());

    // Each group starts from its own copy of the prototype
    std::vector<std::string> lists;
    CHECK(db.prepare("SELECT joined(name) FROM (SELECT * FROM score ORDER BY name) GROUP BY team ORDER BY team")
              .and_then(
                  sqlnx::sqlite::for_each_row<std::string>(
                      [&](const std::string& list)
                      {
                          lists.push_back(list);
                      }
                  )
              )
              .has_value());
    CHECK(lists.size() == 2);
    CHECK(lists.size() == 2 && lists[0] == "Cid, Eve" && lists[1] == "Ann, Bob, Dee");

    // The prototype itself is untouched; another registration has its own settings
    CHECK(prototype.text.empty());
    joined dashes;
    dashes.separator = "-";
    CHECK(db.create_aggregate("dashed", dashes).has_value());
    expected<std::string> all = select<std::string>(db, "SELECT dashed(name) FROM (SELECT * FROM score ORDER BY name)");
    CHECK(all && *all == "Ann-Bob-Cid-Dee-Eve");
}

void test_null_and_empty()
{
    database db = open_scores();
    CHECK(db.create_aggregate("counted", counted()).has_value());

    expected<std::int64_t> red = select<std::int64_t>(db, "SELECT counted(points) FROM score WHERE team = 'red'");
    CHECK(red && *red == 2);

    // No rows: finish() runs on a fresh copy
    expected<std::int64_t> none = select<std::int64_t>(db, "SELECT counted(points) FROM score WHERE team = 'green'");
    CHECK(none && *none == 0);

    // NULL into a parameter that isn't an optional fails the query
    CHECK(db.create_aggregate("joined", joined()).has_value());
    expected<std::string> strict = select<std::string>(db, "SELECT joined(points) FROM score");
    CHECK(!strict && strict.error().primary() == primary_code::mismatch);
    CHECK(!strict && strict.error().message() == "argument 1 is NULL; take it as an optional to accept NULL");
}

void test_errors()
{
    database db = open_scores();
    checked_sum prototype;
    prototype.limit = 10;
    CHECK(db.create_aggregate("checked_sum", prototype).has_value());

    expected<std::int64_t> fine = select<std::int64_t>(db, "SELECT checked_sum(points) FROM score WHERE team = 'blue'");
    CHECK(fine && *fine == 7);

    // step() failing fails the query with its code and message
    expected<std::int64_t> negative = select<std::int64_t>(db, "SELECT checked_sum(-1)");
    CHECK(!negative && negative.error().code() == extended_code::constraint_check);
    CHECK(!negative && negative.error().message() == "checked_sum: negative value");

    // So does finish(): 3 + 5 is within the limit, 13 + 15 is not
    expected<std::int64_t> within = select<std::int64_t>(db, "SELECT checked_sum(points) FROM score WHERE points > 2");
    CHECK(within && *within == 8);
    expected<std::int64_t> over =
        select<std::int64_t>(db, "SELECT checked_sum(points + 10) FROM score WHERE points > 2");
    CHECK(!over && over.error().primary() == primary_code::range);
    CHECK(!over && over.error().message() == "checked_sum: over the limit");

    // A plain aggregate is no window function
    expected<std::int64_t> windowed =
        select<std::int64_t>(db, "SELECT checked_sum(points) OVER (ORDER BY name) FROM score WHERE points IS NOT NULL");
    CHECK(!windowed && windowed.error().primary() == primary_code::error);

    // No database, no aggregate
    database closed;
    expected<void> nowhere = closed.create_aggregate("counted", counted());
    CHECK(!nowhere && nowhere.error().primary() == primary_code::misuse);
}

void test_window()
{
    database db = open_scores();
    CHECK(db.create_aggregate("moving_sum", moving_sum()).has_value());

    // Two rows at a time: inverse() takes the oldest back out
    std::vector<std::int64_t> sums;
    CHECK(db.prepare(
                "SELECT moving_sum(points) OVER (ORDER BY name ROWS BETWEEN 1 PRECEDING AND CURRENT ROW)"
                " FROM score WHERE points IS NOT NULL ORDER BY name"
    )
              .and_then(
                  sqlnx::sqlite::for_each_row<std::int64_t>(
                      [&](std::int64_t sum)
                      {
                          sums.push_back(sum);
                      }
                  )
              )
              .has_value());
    // Ann 3, Cid 5, Dee 1, Eve 2
    CHECK(sums.size() == 4);
    CHECK(sums.size() == 4 && sums[0] == 3 && sums[1] == 8 && sums[2] == 6 && sums[3] == 3);

    // Still an ordinary aggregate too
    expected<std::int64_t> total = select<std::int64_t>(db, "SELECT moving_sum(points) FROM score WHERE points > 0");
    CHECK(total && *total == 11);
}

void test_lifetime()
{
    std::shared_ptr<int> token = std::make_shared<int>(0);
    {
        database db = open_scores();
        tracked prototype;
        prototype.token = token;
        CHECK(db.create_aggregate("tracked", prototype).has_value());
        // Ours and the one SQLite keeps
        CHECK(token.use_count() == 3);

        // The groups' copies are gone once the query is done
        CHECK(db.execute("SELECT tracked(1) FROM score GROUP BY team").has_value());
        CHECK(token.use_count() == 3);

        // Also when a query stops early
        expected<sqlnx::sqlite::statement> partial = db.prepare("SELECT team, tracked(1) FROM score GROUP BY team");
        CHECK(partial && partial->step().has_value());
        CHECK(partial && partial->reset().has_value());
        CHECK(token.use_count() == 3);
    }
    // Closing the database releases SQLite's copy (and ours went with the scope)
    CHECK(token.use_count() == 1);
}

#if SQLNX_TEST_EXCEPTIONS
void test_exceptions()
{
    database db = open_scores();
    CHECK(db.create_aggregate("throwing", throwing()).has_value());
    expected<int> failed = select<int>(db, "SELECT throwing(points) FROM score WHERE points IS NOT NULL");
    CHECK(!failed && failed.error().primary() == primary_code::error);
    CHECK(!failed && failed.error().message() == "throwing: no twos");
}
#endif
} // anonymous

int main()
{
    test_groups_and_prototype();
    test_null_and_empty();
    test_errors();
    test_window();
    test_lifetime();
#if SQLNX_TEST_EXCEPTIONS
    test_exceptions();
#endif
    return test::summary();
}
