// Tests for statement::rows(): the rows of a query as a range.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if SQLITEXX_CPLUSPLUS >= 202002L && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::optional;
using sqlitexx::rows_range;
using sqlitexx::statement;
using sqlitexx::native::primary_code;

namespace
{
database open_users()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    CHECK(db->execute(
                "CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INTEGER);"
                "INSERT INTO user(name, age) VALUES('Alice', 31), ('Bob', NULL), ('Carol', 25);"
    )
              .has_value());
    return std::move(*db);
}

statement prepare(
    database& db,
    const char* sql
)
{
    expected<statement> prepared = db.prepare(sql);
    CHECK(prepared.has_value());
    if (!prepared)
    {
        return statement();
    }
    return std::move(*prepared);
}

void test_tuples()
{
    database db = open_users();
    statement select = prepare(db, "SELECT name, age FROM user ORDER BY id");

    std::vector<std::string> names;
    int known_ages = 0;
    for (expected<std::tuple<std::string, optional<int>>>& row : select.rows<std::string, optional<int>>())
    {
        CHECK(row.has_value());
        if (!row)
        {
            break;
        }
        // Elements are mutable: values can be moved out
        names.push_back(std::move(std::get<0>(*row)));
        if (std::get<1>(*row))
        {
            ++known_ages;
        }
    }
    CHECK(names.size() == 3);
    CHECK(names.size() == 3 && names[0] == "Alice" && names[1] == "Bob" && names[2] == "Carol");
    CHECK(known_ages == 2);
}

void test_single_column()
{
    database db = open_users();
    statement select = prepare(db, "SELECT id FROM user ORDER BY id");

    long long sum = 0;
    for (const expected<int>& id : select.rows<int>())
    {
        CHECK(id.has_value());
        if (id)
        {
            sum += *id;
        }
    }
    CHECK(sum == 6);
}

void test_no_rows()
{
    database db = open_users();
    statement select = prepare(db, "SELECT id FROM user WHERE id > 100");

    rows_range<int> rows = select.rows<int>();
    CHECK(rows.begin() == rows.end());
    CHECK(!(rows.begin() != rows.end()));
}

void test_read_error_is_one_row()
{
    database db = open_users();
    statement select = prepare(db, "SELECT name, age FROM user ORDER BY id");

    // Bob's age is NULL: his row is an error, the others are not
    std::vector<bool> ok;
    for (const expected<std::tuple<std::string, int>>& row : select.rows<std::string, int>())
    {
        ok.push_back(row.has_value());
        if (!row)
        {
            CHECK(row.error().primary() == primary_code::mismatch);
        }
    }
    CHECK(ok.size() == 3);
    CHECK(ok.size() == 3 && ok[0] && !ok[1] && ok[2]);
}

void test_step_error_ends_the_range()
{
    database db = open_users();
    // abs() of the smallest integer overflows: stepping to the second row fails
    statement select =
        prepare(db, "SELECT CASE WHEN id = 2 THEN abs(-9223372036854775807 - 1) ELSE id END FROM user ORDER BY id");

    std::vector<int> ids;
    int errors = 0;
    for (const expected<int>& id : select.rows<int>())
    {
        if (id)
        {
            ids.push_back(*id);
        }
        else
        {
            ++errors;
            CHECK(id.error().primary() == primary_code::error);
            CHECK(!id.error().message().empty());
        }
    }
    CHECK(ids.size() == 1 && ids[0] == 1);
    CHECK(errors == 1);
}

void test_continues_where_the_statement_is()
{
    database db = open_users();
    statement select = prepare(db, "SELECT id FROM user ORDER BY id");

    expected<int> first = select.first<int>();
    CHECK(first && *first == 1);

    std::vector<int> rest;
    for (const expected<int>& id : select.rows<int>())
    {
        if (id)
        {
            rest.push_back(*id);
        }
    }
    CHECK(rest.size() == 2 && rest[0] == 2 && rest[1] == 3);

    // After a reset the query runs again from the start
    CHECK(select.reset().has_value());
    rows_range<int> again = select.rows<int>();
    CHECK(
        std::count_if(
            again.begin(),
            again.end(),
            [](const expected<int>& id)
            {
                return id.has_value();
            }
        )
        == 3
    );
}

void test_iterator_operations()
{
    database db = open_users();
    statement select = prepare(db, "SELECT id FROM user ORDER BY id");

    rows_range<int> rows = select.rows<int>();
    rows_range<int>::iterator it = rows.begin();
    CHECK(it != rows.end());
    CHECK(it->has_value() && **it == 1);

    // *it++ gives the row it moved past
    expected<int> passed = *it++;
    CHECK(passed && *passed == 1);
    CHECK(it != rows.end() && **it == 2);

    ++it;
    CHECK(**it == 3);
    ++it;
    CHECK(it == rows.end());
    CHECK(it == rows_range<int>::iterator());
}

#if defined(__cpp_lib_ranges)
static_assert(std::input_iterator<rows_range<int>::iterator>);
static_assert(std::ranges::input_range<rows_range<int>>);
static_assert(std::ranges::input_range<rows_range<
                  std::string,
                  optional<int>>>);

void test_cxx20_views()
{
    database db = open_users();
    statement select = prepare(db, "SELECT id FROM user ORDER BY id");

    std::vector<int> taken;
    for (const expected<int>& id : select.rows<int>() | std::views::take(2))
    {
        if (id)
        {
            taken.push_back(*id);
        }
    }
    CHECK(taken.size() == 2 && taken[0] == 1 && taken[1] == 2);

    CHECK(select.reset().has_value());
    rows_range<int> rows = select.rows<int>();
    auto odd = rows
        | std::views::filter(
                   [](const expected<int>& id)
                   {
                       return id && *id % 2 == 1;
                   }
        )
        | std::views::transform(
                   [](const expected<int>& id)
                   {
                       return *id;
                   }
        );
    std::vector<int> odds;
    for (int id : odd)
    {
        odds.push_back(id);
    }
    CHECK(odds.size() == 2 && odds[0] == 1 && odds[1] == 3);
}
#endif
} // anonymous

int main()
{
    test_tuples();
    test_single_column();
    test_no_rows();
    test_read_error_is_one_row();
    test_step_error_ends_the_range();
    test_continues_where_the_statement_is();
    test_iterator_operations();
#if defined(__cpp_lib_ranges)
    test_cxx20_views();
#endif
    return test::summary();
}
