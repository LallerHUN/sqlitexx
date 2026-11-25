// Tests for rows as structs: SQLITEXX_FIELDS, hand-written field lists, and reading them everywhere a row is read.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::optional;
using sqlitexx::statement;
using sqlitexx::native::primary_code;

namespace app
{
struct user
{
    std::string name;
    optional<int> age;
    std::int64_t id = 0;
};
SQLITEXX_FIELDS(
    user,
    name,
    age,
    id
)

// A field whose column has another name, written by hand: the macro's C++11 form
struct renamed
{
    std::string who;
    int years = 0;
};

inline auto sqlitexx_fields(const renamed*) -> decltype(sqlitexx::fields(
    sqlitexx::field(
        "name",
        &renamed::who
    ),
    sqlitexx::field(
        "age",
        &renamed::years
    )
))
{
    return sqlitexx::fields(sqlitexx::field("name", &renamed::who), sqlitexx::field("age", &renamed::years));
}

// A view member makes the struct a view
struct name_view
{
    sqlitexx::string_view name;
};
SQLITEXX_FIELDS(
    name_view,
    name
)
} // app

// In the global namespace, found the same way
struct counted
{
    std::int64_t n = 0;
};
SQLITEXX_FIELDS(
    counted,
    n
)

static_assert(
    sqlitexx::detail::has_fields<app::user>::value,
    "user has fields"
);
static_assert(
    !sqlitexx::detail::has_fields<int>::value,
    "int has none"
);
static_assert(
    sqlitexx::detail::reads_view<app::name_view>::value,
    "a struct with a view member is a view"
);
static_assert(
    !sqlitexx::detail::reads_view<app::user>::value,
    "user owns its values"
);

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
                "CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INTEGER, note TEXT);"
                "INSERT INTO user(name, age, note) VALUES('Alice', 31, 'x'), ('Bob', NULL, 'y'), ('Carol', 25, 'z');"
    )
              .has_value());
    return std::move(*db);
}

void test_query()
{
    database db = open_users();

    // By name: the column order doesn't matter, and the extra column (note) is ignored
    std::vector<std::string> names;
    for (expected<app::user>& u : db.query<app::user>("SELECT note, age, id, name FROM user ORDER BY id"))
    {
        CHECK(u.has_value());
        if (u)
        {
            names.push_back(u->name);
        }
    }
    CHECK((names == std::vector<std::string>{"Alice", "Bob", "Carol"}));

    expected<app::user> bob = db.query<app::user>("SELECT * FROM user WHERE name = ?", "Bob").first();
    CHECK(bob && bob->name == "Bob" && !bob->age && bob->id == 2);

    expected<std::vector<app::user>> all = db.query<app::user>("SELECT * FROM user ORDER BY id").all();
    CHECK(all && all->size() == 3 && (*all)[2].age && *(*all)[2].age == 25);

    // Names compare like SQL identifiers, and an alias fits a column to a field
    expected<counted> count = db.query<counted>("SELECT count(*) AS N FROM user").first();
    CHECK(count && count->n == 3);
}

void test_hand_written_fields()
{
    database db = open_users();
    expected<app::renamed> alice = db.query<app::renamed>("SELECT name, age FROM user WHERE id = 1").first();
    CHECK(alice && alice->who == "Alice" && alice->years == 31);
}

void test_errors()
{
    database db = open_users();

    // A field without a column names it
    expected<app::user> missing = db.query<app::user>("SELECT name, age FROM user").first();
    CHECK(!missing && missing.error().primary() == primary_code::range);
    CHECK(!missing && missing.error().message().find("id") != std::string::npos);

    // NULL into a non-optional field: Bob's age into an int
    expected<app::renamed> bob = db.query<app::renamed>("SELECT name, age FROM user WHERE id = 2").first();
    CHECK(!bob && bob.error().primary() == primary_code::mismatch);
}

void test_everywhere_a_row_is_read()
{
    database db = open_users();

    // statement::first and rows, the chaining helpers, for_each
    expected<statement> select = db.prepare("SELECT * FROM user WHERE id >= ? ORDER BY id", 2);
    CHECK(select.has_value());
    if (!select)
    {
        return;
    }
    expected<app::user> first = select->first<app::user>();
    CHECK(first && first->name == "Bob");

    CHECK(select->reset().has_value());
    int seen = 0;
    for (expected<app::user>& u : select->rows<app::user>())
    {
        CHECK(u.has_value());
        ++seen;
    }
    CHECK(seen == 2);

    expected<app::user> chained =
        db.prepare("SELECT * FROM user WHERE id = ?", 3).and_then(sqlitexx::first<app::user>());
    CHECK(chained && chained->name == "Carol");

    std::vector<std::string> names;
    CHECK(db.prepare("SELECT * FROM user ORDER BY id")
              .and_then(
                  sqlitexx::for_each_row<app::user>(
                      [&names](const app::user& u)
                      {
                          names.push_back(u.name);
                      }
                  )
              )
              .has_value());
    CHECK(names.size() == 3);

    // Pages
    sqlitexx::results<app::user> users = db.query<app::user>("SELECT * FROM user ORDER BY id");
    expected<std::vector<app::user>> page = users.next(2);
    CHECK(page && page->size() == 2);

    // A struct with a view member reads views on a named results, valid until its next run
    sqlitexx::results<app::name_view> views = db.query<app::name_view>("SELECT name FROM user ORDER BY id");
    expected<app::name_view> view = views.first();
    CHECK(view && view->name == "Alice");
}
} // anonymous

int main()
{
    test_query();
    test_hand_written_fields();
    test_errors();
    test_everywhere_a_row_is_read();

    return test::summary();
}
