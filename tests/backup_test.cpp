// Tests for database::backup_to and begin_backup_to: copying a whole database into another.

#include "check.hpp"
#include "sqlnx/sqlite.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

using sqlnx::sqlite::backup;
using sqlnx::sqlite::blob;
using sqlnx::sqlite::database;
using sqlnx::sqlite::expected;
using sqlnx::sqlite::native::primary_code;

namespace
{
// Saving to a file needs one; it is created in the working directory and removed afterwards
const char* const path = "sqlnx_backup_test.sqlite";

database open(const char* filename)
{
    expected<database> db = database::open(filename);
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    return std::move(*db);
}

std::int64_t count_items(database& db)
{
    return db.prepare("SELECT count(*) FROM item").and_then(sqlnx::sqlite::first<std::int64_t>()).value_or(-1);
}

// A source with `rows` rows of 1 KiB each, so that it spans many pages
database filled(int rows)
{
    database db = open(":memory:");
    CHECK(db.execute("CREATE TABLE item(id INTEGER PRIMARY KEY, data BLOB)").has_value());
    expected<sqlnx::sqlite::statement> insert = db.prepare("INSERT INTO item(data) VALUES(?)");
    CHECK(insert.has_value());
    for (int i = 0; insert && i < rows; ++i)
    {
        CHECK(insert->execute(blob(1024, static_cast<unsigned char>(i))).has_value());
    }
    return db;
}

void test_copy_all_at_once()
{
    database source = filled(10);
    database destination = open(":memory:");
    CHECK(destination.execute("CREATE TABLE replaced(x)").has_value());

    CHECK(source.backup_to(destination).has_value());
    CHECK(count_items(destination) == 10);

    // The destination's content is replaced, not merged
    CHECK(!destination.execute("SELECT * FROM replaced").has_value());

    // The source is untouched
    CHECK(count_items(source) == 10);
}

void test_save_and_restore_a_file()
{
    std::remove(path);
    {
        database memory = filled(3);
        database file = open(path);
        CHECK(memory.backup_to(file).has_value());
    }
    {
        // Restoring is the same call the other way round
        database file = open(path);
        database memory = open(":memory:");
        CHECK(file.backup_to(memory).has_value());
        CHECK(count_items(memory) == 3);
    }
    std::remove(path);
}

void test_in_steps()
{
    database source = filled(200);
    database destination = open(":memory:");

    expected<backup> copy = source.begin_backup_to(destination);
    CHECK(copy.has_value());
    if (!copy)
    {
        return;
    }

    // Progress is known after the first step
    int steps = 0;
    int last_remaining = -1;
    bool finished = false;
    while (!finished && steps < 10000)
    {
        expected<bool> done = copy->step(10);
        CHECK(done.has_value());
        if (!done)
        {
            break;
        }
        finished = *done;
        ++steps;
        CHECK(copy->page_count() > 0);
        CHECK(last_remaining < 0 || copy->remaining() < last_remaining);
        last_remaining = copy->remaining();
    }
    CHECK(finished);
    CHECK(steps > 1);
    CHECK(copy->remaining() == 0);

    // Stepping after the end keeps reporting the end
    expected<bool> again = copy->step();
    CHECK(again && *again);

    CHECK(count_items(destination) == 200);
}

void test_errors()
{
    database source = filled(1);
    database destination = open(":memory:");

    // An unknown schema
    expected<void> unknown = source.backup_to(destination, "nowhere");
    CHECK(!unknown && unknown.error().primary() == primary_code::error);
    CHECK(!unknown && unknown.error().message().find("nowhere") != std::string::npos);

    // A database cannot be copied over itself
    expected<void> itself = source.backup_to(source);
    CHECK(!itself && itself.error().primary() == primary_code::error);

    // Both databases must be open
    database closed;
    expected<void> to_closed = source.backup_to(closed);
    CHECK(!to_closed && to_closed.error().primary() == primary_code::misuse);
    expected<backup> from_closed = closed.begin_backup_to(destination);
    CHECK(!from_closed && from_closed.error().primary() == primary_code::misuse);

    // A default-constructed backup has nothing to copy
    backup none;
    expected<bool> stepped = none.step();
    CHECK(!stepped && stepped.error().primary() == primary_code::misuse);
    CHECK(none.remaining() == 0 && none.page_count() == 0);
}
} // anonymous

int main()
{
    test_copy_all_at_once();
    test_save_and_restore_a_file();
    test_in_steps();
    test_errors();
    return test::summary();
}
