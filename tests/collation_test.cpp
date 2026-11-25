// Tests for database::create_collation and connection::create_collation: collations written in C++.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::statement;
using sqlitexx::string_view;
using sqlitexx::native::extended_code;
using sqlitexx::native::primary_code;

namespace
{
database open_memory()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    return std::move(*db);
}

// The rows of a one-column query, joined with commas, or "error"
std::string joined(
    database& db,
    const char* sql
)
{
    std::string text;
    expected<void> done = db.prepare(sql).and_then(
        sqlitexx::for_each_row<std::string>(
            [&text](const std::string& value)
            {
                text += text.empty() ? value : "," + value;
            }
        )
    );
    return done ? text : "error";
}

char lower_ascii(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// ASCII letters without regard to case, the rest byte by byte
int compare_nocase(
    string_view first,
    string_view second
)
{
    const std::size_t common = first.size() < second.size() ? first.size() : second.size();
    for (std::size_t i = 0; i != common; ++i)
    {
        const unsigned char a = static_cast<unsigned char>(lower_ascii(first[i]));
        const unsigned char b = static_cast<unsigned char>(lower_ascii(second[i]));
        if (a != b)
        {
            return a < b ? -1 : 1;
        }
    }
    return first.size() == second.size() ? 0 : (first.size() < second.size() ? -1 : 1);
}

database open_fruit()
{
    database db = open_memory();
    CHECK(db.execute(
                "CREATE TABLE fruit(name TEXT);"
                "INSERT INTO fruit VALUES('banana'), ('Apple'), ('cherry'), ('apricot')"
    )
              .has_value());
    return db;
}

void test_order_by()
{
    database db = open_fruit();
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name") == "Apple,apricot,banana,cherry");

    CHECK(db.create_collation("nocase_ascii", &compare_nocase).has_value());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE nocase_ascii") == "Apple,apricot,banana,cherry");

    // A lambda, and a result type other than int: by length, then by bytes
    CHECK(db.create_collation(
                "by_length",
                [](string_view first, string_view second) -> long
                {
                    if (first.size() != second.size())
                    {
                        return first.size() < second.size() ? -1L : 1L;
                    }
                    return static_cast<long>(first.compare(second));
                }
    ).has_value());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE by_length") == "Apple,banana,cherry,apricot");
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE by_length DESC") == "apricot,cherry,banana,Apple");
}

void test_equality()
{
    database db = open_fruit();
    CHECK(db.create_collation("nocase_ascii", &compare_nocase).has_value());

    expected<int> same = db.prepare("SELECT 'APPLE' = 'apple' COLLATE nocase_ascii").and_then(sqlitexx::first<int>());
    CHECK(same && *same == 1);
    expected<int> count = db.prepare("SELECT count(*) FROM fruit WHERE name = 'apple' COLLATE nocase_ascii")
                              .and_then(sqlitexx::first<int>());
    CHECK(count && *count == 1);

    // In a column definition, UNIQUE follows the collation
    CHECK(
        db.execute("CREATE TABLE tag(name TEXT COLLATE nocase_ascii UNIQUE); INSERT INTO tag VALUES('Red')").has_value()
    );
    expected<void> duplicate = db.execute("INSERT INTO tag VALUES('RED')");
    CHECK(!duplicate && duplicate.error().code() == extended_code::constraint_unique);
}

void test_texts_arrive_whole()
{
    database db = open_memory();
    std::size_t longest = 0;
    bool saw_empty = false;
    CHECK(db.create_collation(
                "measuring",
                [&longest, &saw_empty](string_view first, string_view second)
                {
                    for (string_view text : {first, second})
                    {
                        longest = text.size() > longest ? text.size() : longest;
                        saw_empty = saw_empty || text.empty();
                    }
                    return first.compare(second);
                }
    ).has_value());

    // UTF-8 arrives as bytes: an e with an acute accent is two
    expected<int> order = db.prepare("SELECT '' < 'caf\xC3\xA9' COLLATE measuring").and_then(sqlitexx::first<int>());
    CHECK(order && *order == 1);
    CHECK(longest == 5);
    CHECK(saw_empty);
}

// A move-only function object with state: SQLite keeps the one moved in
struct reversed
{
    std::unique_ptr<int> calls;

    int operator()(
        string_view first,
        string_view second
    ) const
    {
        ++*calls;
        return second.compare(first);
    }
};

void test_move_only()
{
    database db = open_fruit();
    reversed order{std::unique_ptr<int>(new int(0))};
    const int* calls = order.calls.get();
    CHECK(db.create_collation("reversed", std::move(order)).has_value());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE reversed") == "cherry,banana,apricot,Apple");
    CHECK(*calls > 0);
}

#if defined(__cpp_lib_three_way_comparison) && defined(__cpp_lib_string_view)
void test_three_way()
{
    database db = open_fruit();
    CHECK(db.create_collation(
                "spaceship",
                [](string_view first, string_view second)
                {
                    return first <=> second;
                }
    ).has_value());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE spaceship") == "Apple,apricot,banana,cherry");
}
#endif

// Counts how many copies SQLite released
struct counted
{
    int* released;
    int direction;

    counted(
        int* released_count,
        int order
    )
        : released(released_count),
          direction(order)
    {
    }

    counted(counted&& other) noexcept
        : released(other.released),
          direction(other.direction)
    {
        other.released = nullptr;
    }

    counted(const counted&) = delete;
    counted& operator=(const counted&) = delete;
    counted& operator=(counted&&) = delete;

    ~counted()
    {
        if (released != nullptr)
        {
            ++*released;
        }
    }

    int operator()(
        string_view first,
        string_view second
    ) const
    {
        return direction * first.compare(second);
    }
};

void test_replace_and_release()
{
    int released = 0;
    {
        database db = open_fruit();
        CHECK(db.create_collation("ordered", counted(&released, 1)).has_value());
        CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE ordered") == "Apple,apricot,banana,cherry");

        // Replacing releases the old copy
        CHECK(db.create_collation("ORDERED", counted(&released, -1)).has_value());
        CHECK(released == 1);
        CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE ordered") == "cherry,banana,apricot,Apple");

        // Not while a statement runs: busy, the old collation stays, and the new copy is released
        expected<statement> running = db.prepare("SELECT name FROM fruit ORDER BY name COLLATE ordered");
        CHECK(running.has_value());
        expected<bool> row = running->step();
        CHECK(row && *row);
        expected<void> refused = db.create_collation("ordered", counted(&released, 1));
        CHECK(!refused && refused.error().primary() == primary_code::busy);
        CHECK(released == 2);
        expected<std::string> first = running->column<std::string>(0);
        CHECK(first && *first == "cherry");
    }
    // Closing the database releases the last one
    CHECK(released == 3);
}

void test_closed_database()
{
    database closed;
    expected<void> registered = closed.create_collation("nocase_ascii", &compare_nocase);
    CHECK(!registered && registered.error().primary() == primary_code::misuse);
}

void test_remove()
{
    int released = 0;
    database db = open_fruit();
    CHECK(db.create_collation("ordered", counted(&released, -1)).has_value());

    // Not while a statement runs: busy, and the collation stays
    expected<statement> running = db.prepare("SELECT name FROM fruit ORDER BY name COLLATE ordered");
    CHECK(running.has_value());
    expected<bool> row = running->step();
    CHECK(row && *row);
    expected<void> refused = db.remove_collation("ordered");
    CHECK(!refused && refused.error().primary() == primary_code::busy);
    CHECK(released == 0);
    CHECK(running->reset().has_value());

    // Removing releases SQLite's copy at once, and statements using the name fail
    CHECK(db.remove_collation("ORDERED").has_value());
    CHECK(released == 1);
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE ordered") == "error");
    CHECK(!running->step());

    // Removing what isn't there does nothing, and a removed name can be registered again
    CHECK(db.remove_collation("never_registered").has_value());
    CHECK(db.create_collation("ordered", counted(&released, 1)).has_value());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE ordered") == "Apple,apricot,banana,cherry");

    database closed;
    expected<void> not_open = closed.remove_collation("ordered");
    CHECK(!not_open && not_open.error().primary() == primary_code::misuse);
}

// What removing is for: a function and a collation that share state outside them, by reference. Removing both
// before the state ends means SQLite never holds a reference to it past its life.
struct sort_settings
{
    bool descending = false;
};

void test_shared_state()
{
    database db = open_fruit();
    {
        sort_settings settings;
        CHECK(db.create_collation(
                    "configured",
                    [&settings](string_view first, string_view second)
                    {
                        return settings.descending ? second.compare(first) : first.compare(second);
                    }
        ).has_value());
        CHECK(db.create_function(
                    "set_descending",
                    [&settings](bool descending)
                    {
                        settings.descending = descending;
                    }
        ).has_value());

        CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE configured") == "Apple,apricot,banana,cherry");
        CHECK(db.execute("SELECT set_descending(1)").has_value());
        CHECK(settings.descending);
        CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE configured") == "cherry,banana,apricot,Apple");

        CHECK(db.remove_collation("configured").has_value());
        CHECK(db.remove_function("set_descending", 1).has_value());
    }
    // settings is gone, and nothing in SQLite refers to it
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE configured") == "error");
}

void test_unknown_collation()
{
    database db = open_fruit();
    expected<statement> unknown = db.prepare("SELECT name FROM fruit ORDER BY name COLLATE missing");
    CHECK(!unknown && unknown.error().primary() == primary_code::error);
}

// The native layer takes a C callback and a user_data pointer, and a null callback removes the collation
int compare_native(
    void* user_data,
    int first_size,
    const void* first,
    int second_size,
    const void* second
)
{
    ++*static_cast<int*>(user_data);
    const int common = first_size < second_size ? first_size : second_size;
    const int bytes = std::memcmp(first, second, static_cast<std::size_t>(common));
    return bytes != 0 ? bytes : first_size - second_size;
}

void test_native()
{
    database db = open_fruit();
    sqlitexx::native::connection& native = db.native_connection();
    int calls = 0;
    CHECK(native.create_collation("bytes", &calls, &compare_native, nullptr).ok());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE bytes") == "Apple,apricot,banana,cherry");
    CHECK(calls > 0);

    CHECK(native.create_collation("bytes", nullptr, nullptr, nullptr).ok());
    CHECK(joined(db, "SELECT name FROM fruit ORDER BY name COLLATE bytes") == "error");
}

// What is and isn't a collation, at compile time
// clang-format off
static_assert(sqlitexx::detail::is_collation<decltype(&compare_nocase)>::value, "an int-returning function");
static_assert(sqlitexx::detail::is_collation<reversed>::value, "a function object");
static_assert(!sqlitexx::detail::is_collation<int (*)(int)>::value, "wrong parameters");
static_assert(!sqlitexx::detail::is_collation<bool (*)(string_view, string_view)>::value, "a less-than predicate");
static_assert(!sqlitexx::detail::is_collation<unsigned (*)(string_view, string_view)>::value, "can't be negative");
// clang-format on
} // anonymous

int main()
{
    test_order_by();
    test_equality();
    test_texts_arrive_whole();
    test_move_only();
#if defined(__cpp_lib_three_way_comparison) && defined(__cpp_lib_string_view)
    test_three_way();
#endif
    test_replace_and_release();
    test_closed_database();
    test_remove();
    test_shared_state();
    test_unknown_collation();
    test_native();
    return test::summary();
}
