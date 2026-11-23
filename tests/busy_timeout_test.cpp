// Tests for database::busy_timeout: waiting for another connection's lock.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <chrono>
#include <cstdio>
#include <utility>

using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::native::primary_code;

namespace
{
// Two connections need a database file; it is created in the working directory and removed afterwards
const char* const path = "sqlitexx_busy_timeout_test.sqlite";

void remove_database()
{
    std::remove(path);
    std::remove("sqlitexx_busy_timeout_test.sqlite-journal");
}

database open_file()
{
    expected<database> db = database::open(path);
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    return std::move(*db);
}

// How long `waiting` takes to give up on BEGIN IMMEDIATE while another connection holds the write lock
std::chrono::milliseconds time_busy_begin(database& waiting)
{
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    expected<void> begun = waiting.execute("BEGIN IMMEDIATE");
    const std::chrono::steady_clock::time_point stop = std::chrono::steady_clock::now();
    CHECK(!begun);
    CHECK(!begun && begun.error().primary() == primary_code::busy);
    return std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
}

void test_waits_then_fails_busy()
{
    remove_database();
    {
        database holder = open_file();
        database waiting = open_file();
        CHECK(holder.execute("CREATE TABLE item(id INTEGER PRIMARY KEY)").has_value());
        CHECK(holder.execute("BEGIN IMMEDIATE").has_value());

        // No timeout (the default): busy at once
        CHECK(time_busy_begin(waiting) < std::chrono::milliseconds(150));

        // With a timeout: busy only after waiting for it
        CHECK(waiting.busy_timeout(std::chrono::milliseconds(200)).has_value());
        CHECK(time_busy_begin(waiting) >= std::chrono::milliseconds(150));

        // Zero turns waiting off again; so does a negative timeout
        CHECK(waiting.busy_timeout(std::chrono::milliseconds(0)).has_value());
        CHECK(time_busy_begin(waiting) < std::chrono::milliseconds(150));
        CHECK(waiting.busy_timeout(std::chrono::milliseconds(-5)).has_value());
        CHECK(time_busy_begin(waiting) < std::chrono::milliseconds(150));

        // Once the lock is gone, the other connection gets it
        CHECK(holder.execute("COMMIT").has_value());
        CHECK(waiting.execute("BEGIN IMMEDIATE").has_value());
        CHECK(waiting.execute("COMMIT").has_value());
    }
    remove_database();
}

void test_any_duration()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return;
    }
    CHECK(db->busy_timeout(std::chrono::seconds(5)).has_value());
    // Longer than SQLite takes: waits as long as it can rather than failing or wrapping around
    CHECK(db->busy_timeout(std::chrono::hours(24 * 365)).has_value());
    CHECK(db->busy_timeout((std::chrono::milliseconds::max)()).has_value());
    CHECK(db->busy_timeout((std::chrono::milliseconds::min)()).has_value());
}
} // anonymous

int main()
{
    test_waits_then_fails_busy();
    test_any_duration();
    return test::summary();
}
