// Tests for the typed layer: database, statement, value traits and the chaining helpers.

// sqlitexx must compile after <windows.h> without NOMINMAX, whose min and max macros break unguarded calls such as
// std::numeric_limits<T>::max(). User code includes it first often enough.
#if defined(_WIN32)
#include <windows.h>
#endif

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

using sqlitexx::blob;
using sqlitexx::cstring_view;
using sqlitexx::database;
using sqlitexx::error;
using sqlitexx::expected;
using sqlitexx::optional;
using sqlitexx::statement;
using sqlitexx::string_view;
using sqlitexx::native::primary_code;

namespace
{
// SQLITE_CONSTRAINT_UNIQUE; the tests do not see sqlite3.h, which sqlitexx links privately
const int constraint_unique = 19 | (8 << 8);

// An in-memory database with a user table, or an empty one when that fails (the checks then report it)
database open_users()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    CHECK(db->execute(
                "CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, age INTEGER, photo BLOB);"
                "INSERT INTO user(name, age) VALUES('Alice', 31), ('Bob', NULL);"
    )
              .has_value());
    return std::move(*db);
}

void test_open_failure_has_message()
{
    expected<database> db =
        database::open("this/directory/does/not/exist/db.sqlite", sqlitexx::native::open_flags::readonly);
    CHECK(!db.has_value());
    if (!db)
    {
        CHECK(db.error().primary() == primary_code::cantopen);
        CHECK(!db.error().message().empty());
    }
}

void test_execute_and_first()
{
    database db = open_users();

    expected<statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(?, ?)");
    CHECK(insert.has_value());
    CHECK(insert->execute("Carol", 25).has_value());
    CHECK(db.last_insert_rowid() == 3);
    CHECK(insert->execute(std::string("Dave"), 40).has_value()); // the same statement again, new values
    CHECK(db.changes() == 1);

    expected<statement> select = db.prepare("SELECT name, age FROM user WHERE id = ?");
    CHECK(select.has_value());
    CHECK(select->bind(4).has_value());
    expected<std::tuple<std::string, int>> dave = select->first<std::string, int>();
    CHECK(dave.has_value());
    CHECK(dave && std::get<0>(*dave) == "Dave" && std::get<1>(*dave) == 40);

    expected<statement> count = db.prepare("SELECT count(*) FROM user");
    CHECK(count.has_value());
    expected<std::int64_t> n = count->first<std::int64_t>();
    CHECK(n && *n == 4);
}

void test_null_and_optional()
{
    database db = open_users();
    expected<statement> select = db.prepare("SELECT age FROM user WHERE name = 'Bob'");
    CHECK(select.has_value());

    expected<optional<int>> age = select->first<optional<int>>();
    CHECK(age.has_value());
    CHECK(age && !age->has_value());

    // NULL into a non-optional type is an error, not a silent 0
    expected<int> strict = select->column<int>(0);
    CHECK(!strict.has_value());
    CHECK(!strict && strict.error().primary() == primary_code::mismatch);

    expected<statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(?, ?)");
    CHECK(insert.has_value());
    CHECK(insert->execute("Erin", optional<int>()).has_value());
    CHECK(insert->execute("Frank", nullptr).has_value());
    CHECK(insert->execute("Grace", optional<int>(7)).has_value());

    expected<statement> nulls = db.prepare("SELECT count(*) FROM user WHERE age IS NULL");
    CHECK(nulls.has_value());
    expected<int> null_count = nulls->first<int>();
    CHECK(null_count && *null_count == 3);
}

void test_integer_ranges()
{
    database db = open_users();
    expected<statement> select = db.prepare("SELECT 300, -1, 9223372036854775807");
    CHECK(select.has_value());
    CHECK(select->step().value_or(false));

    CHECK(select->column<std::int16_t>(0).value_or(0) == 300);
    CHECK(!select->column<std::int8_t>(0).has_value()); // 300 does not fit
    CHECK(!select->column<unsigned int>(1).has_value()); // -1 is not unsigned
    CHECK(select->column<std::uint64_t>(2).value_or(0) == 9223372036854775807ULL);
    CHECK(select->column<bool>(0).value_or(false));

    expected<statement> bind = db.prepare("SELECT ?");
    CHECK(bind.has_value());
    expected<void> too_big = bind->bind((std::numeric_limits<std::uint64_t>::max)());
    CHECK(!too_big.has_value());
    CHECK(!too_big && too_big.error().primary() == primary_code::mismatch);
}

void test_blob_bool_double_and_named()
{
    database db = open_users();
    expected<statement> update = db.prepare("UPDATE user SET photo = :photo WHERE name = :name");
    CHECK(update.has_value());
    const blob photo = {0x89, 0x50, 0x4e, 0x47, 0x00, 0xff};
    CHECK(update->bind_named(":photo", photo).has_value());
    CHECK(update->bind_named(":name", "Alice").has_value());
    CHECK(update->step().has_value());
    CHECK(!update->bind_named(":missing", 1).has_value());

    expected<statement> select = db.prepare("SELECT photo, 2.5, 1 FROM user WHERE name = 'Alice'");
    CHECK(select.has_value());
    expected<std::tuple<blob, double, bool>> values = select->first<blob, double, bool>();
    CHECK(values.has_value());
    CHECK(values && std::get<0>(*values) == photo && std::get<1>(*values) == 2.5 && std::get<2>(*values));
}

void test_for_each_and_string_view()
{
    database db = open_users();
    expected<statement> select = db.prepare("SELECT name, age FROM user ORDER BY id");
    CHECK(select.has_value());

    std::string names;
    int known_ages = 0;
    expected<void> done = select->for_each<string_view, optional<int>>(
        [&](string_view name, optional<int> age)
        {
            names += std::string(name.data(), name.size()) + ";";
            if (age)
            {
                ++known_ages;
            }
        }
    );
    CHECK(done.has_value());
    CHECK(names == "Alice;Bob;");
    CHECK(known_ages == 1);
}

void test_cstring_view()
{
    database db = open_users();
    expected<statement> select = db.prepare("SELECT name, age FROM user ORDER BY id");
    CHECK(select.has_value());

    expected<cstring_view> name = select->first<cstring_view>();
    CHECK(name && *name == "Alice");
    CHECK(name && std::strcmp(name->c_str(), "Alice") == 0);

    // Bob's age is NULL: an error for cstring_view, nullopt for optional<cstring_view>
    CHECK(select->step().has_value());
    expected<cstring_view> strict = select->column<cstring_view>(1);
    CHECK(!strict && strict.error().primary() == primary_code::mismatch);
    expected<optional<cstring_view>> age = select->column<optional<cstring_view>>(1);
    CHECK(age && !age->has_value());

    expected<statement> insert = db.prepare("INSERT INTO user(name) VALUES(?)");
    CHECK(insert.has_value());
    CHECK(insert->execute(cstring_view("Carol")).has_value());
    expected<statement> count = db.prepare("SELECT count(*) FROM user WHERE name = 'Carol'");
    CHECK(count.has_value());
    expected<int> carols = count->first<int>();
    CHECK(carols && *carols == 1);
}

void test_errors_carry_codes_and_messages()
{
    database db = open_users();

    expected<statement> duplicate = db.prepare("INSERT INTO user(name) VALUES('Alice')");
    CHECK(duplicate.has_value());
    expected<void> inserted = duplicate->execute();
    CHECK(!inserted.has_value());
    if (!inserted)
    {
        CHECK(inserted.error().primary() == primary_code::constraint);
        CHECK(inserted.error().message().find("UNIQUE") != std::string::npos);
    }

    expected<statement> bad = db.prepare("SELEKT 1");
    CHECK(!bad.has_value());
    CHECK(!bad && bad.error().message().find("syntax error") != std::string::npos);

    // prepare() takes exactly one statement; trailing comments are fine
    CHECK(!db.prepare("SELECT 1; SELECT 2").has_value());
    CHECK(!db.prepare("   ").has_value());
    CHECK(db.prepare("SELECT 1; -- just a comment").has_value());

    expected<statement> none = db.prepare("SELECT name FROM user WHERE id = 99");
    CHECK(none.has_value());
    expected<std::string> missing = none->first<std::string>();
    CHECK(!missing && missing.error().primary() == primary_code::done);
}

void test_chaining()
{
    database db = open_users();

    // Found
    expected<int> alice = db.prepare("SELECT age FROM user WHERE name = ?")
                              .and_then(sqlitexx::bind_values("Alice"))
                              .and_then(sqlitexx::first<int>());
    CHECK(alice && *alice == 31);

    // Not found: on() turns that one error into a value
    expected<int> nobody = db.prepare("SELECT age FROM user WHERE name = ?")
                               .and_then(sqlitexx::bind_values(std::string("Nobody")))
                               .and_then(sqlitexx::first<int>())
                               .or_else(
                                   sqlitexx::on(
                                       primary_code::done,
                                       [](const error&)
                                       {
                                           return expected<int>(0);
                                       }
                                   )
                               );
    CHECK(nobody && *nobody == 0);

    // Other errors pass through on() unchanged
    expected<int> broken = db.prepare("SELEKT age FROM user")
                               .and_then(sqlitexx::first<int>())
                               .or_else(
                                   sqlitexx::on(
                                       primary_code::done,
                                       [](const error&)
                                       {
                                           return expected<int>(0);
                                       }
                                   )
                               );
    CHECK(!broken && broken.error().primary() == primary_code::error);

    // Extended codes, transform and transform_error
    expected<std::string> duplicate = db.prepare("INSERT INTO user(name) VALUES(?)")
                                          .and_then(sqlitexx::execute_with("Bob"))
                                          .transform(
                                              []
                                              {
                                                  return std::string("inserted");
                                              }
                                          )
                                          .or_else(
                                              sqlitexx::on(
                                                  constraint_unique,
                                                  [](const error&)
                                                  {
                                                      return expected<std::string>(std::string("already there"));
                                                  }
                                              )
                                          );
    CHECK(duplicate && *duplicate == "already there");

    expected<int> message_length = db.prepare("SELEKT")
                                       .and_then(sqlitexx::first<int>())
                                       .transform_error(
                                           [](const error& e)
                                           {
                                               return error(e.code(), "wrapped: " + e.message());
                                           }
                                       );
    CHECK(!message_length && message_length.error().message().rfind("wrapped: ", 0) == 0);

    int rows = 0;
    expected<void> counted = db.prepare("SELECT id FROM user")
                                 .and_then(
                                     sqlitexx::for_each_row<std::int64_t>(
                                         [&](std::int64_t)
                                         {
                                             ++rows;
                                         }
                                     )
                                 );
    CHECK(counted.has_value());
    CHECK(rows == 2);
}
} // anonymous

int main()
{
    test_open_failure_has_message();
    test_execute_and_first();
    test_null_and_optional();
    test_integer_ranges();
    test_blob_bool_double_and_named();
    test_for_each_and_string_view();
    test_cstring_view();
    test_errors_carry_codes_and_messages();
    test_chaining();

    return test::summary();
}
