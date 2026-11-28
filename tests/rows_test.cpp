// Tests for statement::rows() and database::query(): the rows of a query as a range.

#include "check.hpp"
#include "sqlnx/sqlite.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if SQLNX_CPLUSPLUS >= 202002L && defined(__has_include)
#if __has_include(<ranges>)
#include <ranges>
#endif
#endif

using sqlnx::sqlite::database;
using sqlnx::sqlite::expected;
using sqlnx::sqlite::optional;
using sqlnx::sqlite::rows_range;
using sqlnx::sqlite::statement;
using sqlnx::sqlite::native::primary_code;

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

void test_query_iterates()
{
    database db = open_users();

    // One line: the results live as long as the loop
    std::vector<std::string> names;
    for (expected<std::tuple<std::string, optional<int>>>& user :
         db.query<std::string, optional<int>>("SELECT name, age FROM user WHERE id >= ? ORDER BY id", 2))
    {
        CHECK(user.has_value());
        if (user)
        {
            names.push_back(std::get<0>(*user));
        }
    }
    CHECK((names == std::vector<std::string>{"Bob", "Carol"}));

    // Views are fine inside the loop: the statement is alive
    int count = 0;
    for (expected<sqlnx::sqlite::string_view>& name : db.query<sqlnx::sqlite::string_view>("SELECT name FROM user"))
    {
        CHECK(name && !name->empty());
        ++count;
    }
    CHECK(count == 3);

    // No rows: the loop body never runs
    int none = 0;
    for (expected<int>& age : db.query<int>("SELECT age FROM user WHERE 0"))
    {
        static_cast<void>(age);
        ++none;
    }
    CHECK(none == 0);
}

void test_query_runs_again()
{
    database db = open_users();
    sqlnx::sqlite::results<std::string> names =
        db.query<std::string>("SELECT name FROM user WHERE age > ? ORDER BY id", 20);

    std::vector<std::string> first_loop;
    for (expected<std::string>& name : names)
    {
        CHECK(name.has_value());
        if (name)
        {
            first_loop.push_back(*name);
        }
        break; // leaving early doesn't matter: the next loop starts over
    }
    CHECK((first_loop == std::vector<std::string>{"Alice"}));

    CHECK(db.execute("INSERT INTO user(name, age) VALUES(?, ?)", "Dee", 41).has_value());
    expected<std::vector<std::string>> all = names.all();
    CHECK(all && (*all == std::vector<std::string>{"Alice", "Carol", "Dee"}));
    expected<std::vector<std::string>> again = names.all();
    CHECK(again && again->size() == 3);
}

void test_query_error_is_the_only_element()
{
    database db = open_users();

    int elements = 0;
    for (expected<int>& row : db.query<int>("SELECT nope FROM user"))
    {
        CHECK(!row && row.error().primary() == primary_code::error);
        ++elements;
    }
    CHECK(elements == 1);

    // Each loop gives the error again
    sqlnx::sqlite::results<int> broken = db.query<int>("SELECT ?", 1, 2);
    for (int loop = 0; loop < 2; ++loop)
    {
        int seen = 0;
        for (expected<int>& row : broken)
        {
            CHECK(!row && row.error().primary() == primary_code::range);
            ++seen;
        }
        CHECK(seen == 1);
    }
    expected<std::vector<int>> all = broken.all();
    CHECK(!all && all.error().primary() == primary_code::range);
}

void test_query_all()
{
    database db = open_users();

    expected<std::vector<std::tuple<std::string, optional<int>>>> users =
        db.query<std::string, optional<int>>("SELECT name, age FROM user ORDER BY id").all();
    CHECK(users && users->size() == 3);
    CHECK(users && std::get<0>((*users)[1]) == "Bob" && !std::get<1>((*users)[1]));

    // A row that can't be read stops it: Bob's age is NULL
    expected<std::vector<int>> ages = db.query<int>("SELECT age FROM user ORDER BY id").all();
    CHECK(!ages && ages.error().primary() == primary_code::mismatch);

    expected<std::vector<int>> none = db.query<int>("SELECT age FROM user WHERE 0").all();
    CHECK(none && none->empty());
}

void test_query_next()
{
    database db = open_users();
    sqlnx::sqlite::results<std::string> names =
        db.query<std::string>("SELECT name FROM user WHERE id >= ? ORDER BY id", 1);

    // Pages of two: Alice and Bob, then Carol, then nothing, and nothing again
    expected<std::vector<std::string>> page = names.next(2);
    CHECK(page && (*page == std::vector<std::string>{"Alice", "Bob"}));
    page = names.next(2);
    CHECK(page && (*page == std::vector<std::string>{"Carol"}));
    page = names.next(2);
    CHECK(page && page->empty());
    page = names.next(2);
    CHECK(page && page->empty());

    // Another run (a loop, first() or all()) starts the pages over
    expected<std::string> first = names.first();
    CHECK(first && *first == "Alice");
    page = names.next(1);
    CHECK(page && (*page == std::vector<std::string>{"Alice"}));
    for (expected<std::string>& name : names)
    {
        static_cast<void>(name);
        break;
    }
    page = names.next(5);
    CHECK(page && page->size() == 3);

    // Zero rows asked: nothing read, nothing moved
    page = names.next(0);
    CHECK(page && page->empty());
}

void test_query_next_errors()
{
    database db = open_users();

    // A row that can't be read fails its page; the next page goes on after it. Bob's age is NULL.
    sqlnx::sqlite::results<int> ages = db.query<int>("SELECT age FROM user ORDER BY id");
    expected<std::vector<int>> page = ages.next(2);
    CHECK(!page && page.error().primary() == primary_code::mismatch);
    page = ages.next(2);
    CHECK(page && (*page == std::vector<int>{25}));

    // A query that couldn't start gives its error every time
    sqlnx::sqlite::results<int> broken = db.query<int>("SELECT nope FROM user");
    expected<std::vector<int>> failed = broken.next(1);
    CHECK(!failed && failed.error().primary() == primary_code::error);
    failed = broken.next(1);
    CHECK(!failed && failed.error().primary() == primary_code::error);
}

void test_query_skip()
{
    database db = open_users();
    CHECK(db.execute("INSERT INTO user(name, age) VALUES('Dee', 41), ('Eve', 29)").has_value());

    // Rows 2 to 4: skip one, read three; skip says how many it passed
    sqlnx::sqlite::results<std::string> names = db.query<std::string>("SELECT name FROM user ORDER BY id");
    expected<std::size_t> skipped = names.skip(1);
    CHECK(skipped && *skipped == 1);
    expected<std::vector<std::string>> page = names.next(3);
    CHECK(page && (*page == std::vector<std::string>{"Bob", "Carol", "Dee"}));

    // Past the end it passes fewer, then none
    skipped = names.skip(5);
    CHECK(skipped && *skipped == 1);
    skipped = names.skip(5);
    CHECK(skipped && *skipped == 0);

    // The same rows with LIMIT and OFFSET
    expected<std::vector<std::string>> same =
        db.query<std::string>("SELECT name FROM user ORDER BY id LIMIT ? OFFSET ?", 3, 1).all();
    CHECK(same && (*same == std::vector<std::string>{"Bob", "Carol", "Dee"}));

    // Skipping reads no columns, so a row that couldn't be read doesn't stop it: Bob's age is NULL
    sqlnx::sqlite::results<int> ages = db.query<int>("SELECT age FROM user ORDER BY id");
    skipped = ages.skip(2);
    CHECK(skipped && *skipped == 2);
    expected<std::vector<int>> rest = ages.next(10);
    CHECK(rest && (*rest == std::vector<int>{25, 41, 29}));

    // A query that couldn't start gives its error
    sqlnx::sqlite::results<int> broken = db.query<int>("SELECT nope FROM user");
    expected<std::size_t> failed = broken.skip(1);
    CHECK(!failed && failed.error().primary() == primary_code::error);
}

#if defined(__cpp_lib_ranges)
static_assert(std::ranges::input_range<sqlnx::sqlite::results<int>>);
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
    test_query_iterates();
    test_query_runs_again();
    test_query_error_is_the_only_element();
    test_query_all();
    test_query_next();
    test_query_next_errors();
    test_query_skip();
#if defined(__cpp_lib_ranges)
    test_cxx20_views();
#endif
    return test::summary();
}
