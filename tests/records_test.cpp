// Tests for records: SQLNX_FIELDS, hand-written field lists, record_traits with record_builder and record_binder,
// reading them everywhere a row is read and binding them as parameters.

#include "check.hpp"
#include "sqlnx/sqlite.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using sqlnx::sqlite::database;
using sqlnx::sqlite::expected;
using sqlnx::sqlite::optional;
using sqlnx::sqlite::statement;
using sqlnx::sqlite::native::primary_code;

namespace app
{
struct user
{
    std::string name;
    optional<int> age;
    std::int64_t id = 0;
};
// clang-format off
SQLNX_FIELDS(user, name, age, id)
// clang-format on

// A field whose column has another name, written by hand: the macro's C++11 form
struct renamed
{
    std::string who;
    int years = 0;
};

inline auto sqlnx_fields(const renamed*) -> decltype(sqlnx::fields(
    sqlnx::field(
        "name",
        &renamed::who
    ),
    sqlnx::field(
        "age",
        &renamed::years
    )
))
{
    return sqlnx::fields(sqlnx::field("name", &renamed::who), sqlnx::field("age", &renamed::years));
}

// A view member makes the struct a view
struct name_view
{
    sqlnx::sqlite::string_view name;
};
// clang-format off
SQLNX_FIELDS(name_view, name)
// clang-format on
} // app

// In the global namespace, found the same way
struct counted
{
    std::int64_t n = 0;
};
// clang-format off
SQLNX_FIELDS(counted, n)
// clang-format on

// No default constructor: SQLNX_FIELDS can't fill it, record_traits with the builder can
struct point
{
    point(
        double from_x,
        double from_y
    )
        : x(from_x),
          y(from_y)
    {
    }

    double x;
    double y;
};

template <>
struct sqlnx::sqlite::record_traits<point>
{
    static expected<point> read(const statement& row)
    {
        return sqlnx::sqlite::record_builder(row).column<double>("x").column<double>("y").build<point>();
    }

    static expected<void> bind(
        statement& stmt,
        const point& p
    )
    {
        return sqlnx::sqlite::record_binder(stmt).bind(":x", p.x).bind(2, p.y).done();
    }
};

// An aggregate built with braces, by column index, and a factory that checks the row
struct span_row
{
    int from;
    int to;
};

template <>
struct sqlnx::sqlite::record_traits<span_row>
{
    static expected<span_row> read(const statement& row)
    {
        return sqlnx::sqlite::record_builder(row).column<int>(0).column<int>(1).build(
            [](int from, int to) -> expected<span_row>
            {
                if (from > to)
                {
                    return expected<span_row>(
                        sqlnx::sqlite::unexpect,
                        sqlnx::sqlite::error(
                            sqlnx::sqlite::native::result(static_cast<int>(primary_code::constraint)),
                            "span ends before it starts"
                        )
                    );
                }
                return span_row{from, to};
            }
        );
    }
};

// A whole class template through a partial specialization
template <class First, class Second>
struct labeled
{
    First label;
    Second value;
};

template <class First, class Second>
struct sqlnx::sqlite::record_traits<labeled<First, Second>>
{
    static expected<labeled<
        First,
        Second>>
    read(const statement& row)
    {
        return sqlnx::sqlite::record_builder(row)
            .template column<First>("label")
            .template column<Second>("value")
            .template build<labeled<First, Second>>();
    }
};

// A hand-written record that keeps a view says so
struct label_view
{
    sqlnx::sqlite::string_view label;
};

template <>
struct sqlnx::sqlite::record_traits<label_view>
{
    static constexpr bool reads_view = true;

    static expected<label_view> read(const statement& row)
    {
        return sqlnx::sqlite::record_builder(row).column<sqlnx::sqlite::string_view>("label").build<label_view>();
    }
};

static_assert(
    sqlnx::sqlite::detail::is_record<point>::value,
    "point has record_traits"
);
static_assert(
    sqlnx::sqlite::detail::is_record<app::user>::value,
    "SQLNX_FIELDS makes a record"
);
static_assert(
    !sqlnx::sqlite::detail::is_record<int>::value,
    "int is a value"
);
static_assert(
    sqlnx::sqlite::detail::reads_view<label_view>::value,
    "label_view says it keeps a view"
);
static_assert(
    !sqlnx::sqlite::detail::reads_view<point>::value,
    "point owns its values"
);

static_assert(
    sqlnx::sqlite::detail::has_fields<app::user>::value,
    "user has fields"
);
static_assert(
    !sqlnx::sqlite::detail::has_fields<int>::value,
    "int has none"
);
static_assert(
    sqlnx::sqlite::detail::reads_view<app::name_view>::value,
    "a struct with a view member is a view"
);
static_assert(
    !sqlnx::sqlite::detail::reads_view<app::user>::value,
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
        db.prepare("SELECT * FROM user WHERE id = ?", 3).and_then(sqlnx::sqlite::first<app::user>());
    CHECK(chained && chained->name == "Carol");

    std::vector<std::string> names;
    CHECK(db.prepare("SELECT * FROM user ORDER BY id")
              .and_then(
                  sqlnx::sqlite::for_each_row<app::user>(
                      [&names](const app::user& u)
                      {
                          names.push_back(u.name);
                      }
                  )
              )
              .has_value());
    CHECK(names.size() == 3);

    // Pages
    sqlnx::sqlite::results<app::user> users = db.query<app::user>("SELECT * FROM user ORDER BY id");
    expected<std::vector<app::user>> page = users.next(2);
    CHECK(page && page->size() == 2);

    // A struct with a view member reads views on a named results, valid until its next run
    sqlnx::sqlite::results<app::name_view> views = db.query<app::name_view>("SELECT name FROM user ORDER BY id");
    expected<app::name_view> view = views.first();
    CHECK(view && view->name == "Alice");
}
void test_record_traits()
{
    database db = open_users();
    CHECK(db.execute(
                "CREATE TABLE shape(x REAL, y REAL, label TEXT, value INTEGER);"
                "INSERT INTO shape VALUES(1.5, 2.5, 'a', 1), (3, NULL, 'b', 2);"
                "CREATE TABLE span(a INTEGER, b INTEGER);"
                "INSERT INTO span VALUES(1, 5), (7, 2);"
    )
              .has_value());

    // A constructor with arguments
    expected<point> first = db.query<point>("SELECT x, y FROM shape ORDER BY rowid").first();
    CHECK(first && first->x == 1.5 && first->y == 2.5);

    // The first error wins: y is NULL in the second row
    expected<std::vector<point>> all = db.query<point>("SELECT y, x FROM shape ORDER BY rowid").all();
    CHECK(!all && all.error().primary() == primary_code::mismatch);
    expected<point> missing = db.query<point>("SELECT x FROM shape").first();
    CHECK(!missing && missing.error().primary() == primary_code::range);

    // By index, braces, and a factory's own error
    std::vector<int> ends;
    int rejected = 0;
    for (expected<span_row>& span : db.query<span_row>("SELECT a, b FROM span ORDER BY rowid"))
    {
        if (span)
        {
            ends.push_back(span->to);
        }
        else
        {
            CHECK(span.error().primary() == primary_code::constraint);
            CHECK(span.error().message() == "span ends before it starts");
            ++rejected;
        }
    }
    CHECK((ends == std::vector<int>{5}) && rejected == 1);

    // A class template
    expected<labeled<std::string, int>> label =
        db.query<labeled<std::string, int>>("SELECT label, value FROM shape WHERE value = ?", 2).first();
    CHECK(label && label->label == "b" && label->value == 2);

    // A hand-written view record on a named results
    sqlnx::sqlite::results<label_view> views = db.query<label_view>("SELECT label FROM shape ORDER BY rowid");
    expected<label_view> view = views.first();
    CHECK(view && view->label == "a");
}

void test_binding_fields()
{
    database db = open_users();
    app::user dee;
    dee.name = "Dee";
    dee.age = 41;

    // Every parameter from the field of its name; fields without a parameter (id) are left out
    CHECK(db.execute("INSERT INTO user(name, age) VALUES(:name, :age)", dee).has_value());
    expected<app::user> read = db.query<app::user>("SELECT * FROM user WHERE name = :name", dee).first();
    CHECK(read && read->age && *read->age == 41 && read->id == 4);

    // The other prefixes, a parameter used twice, and an empty optional binding NULL
    app::user eve;
    eve.name = "Eve";
    CHECK(db.execute("INSERT INTO user(name, age, note) VALUES(@name, $age, :name)", eve).has_value());
    expected<std::int64_t> nulls =
        db.query<std::int64_t>("SELECT count(*) FROM user WHERE name = note AND age IS NULL").first();
    CHECK(nulls && *nulls == 1);

    // prepare with a record, then run it again with another
    expected<statement> insert = db.prepare("INSERT INTO user(name, age) VALUES(:name, :age)", dee);
    CHECK(insert.has_value());
    if (insert)
    {
        CHECK(db.execute("DELETE FROM user WHERE name = ?", "Dee").has_value());
        expected<bool> ran = insert->step(); // runs with dee bound by prepare
        CHECK(ran && !*ran);
        eve.name = "Fay";
        CHECK(insert->execute(eve).has_value());
    }
    expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM user").first();
    CHECK(count && *count == 6);
}

void test_binding_errors()
{
    database db = open_users();
    app::user dee;
    dee.name = "Dee";

    // A parameter without a field, and a positional parameter: never left NULL by accident
    expected<void> unknown = db.execute("INSERT INTO user(name, note) VALUES(:name, :note)", dee);
    CHECK(!unknown && unknown.error().primary() == primary_code::range);
    CHECK(!unknown && unknown.error().message() == "the record has no field for parameter :note");
    expected<void> positional = db.execute("INSERT INTO user(name, age) VALUES(:name, ?)", dee);
    CHECK(!positional && positional.error().primary() == primary_code::range);

    // Field names compare exactly, as SQLite compares parameter names
    expected<void> wrong_case = db.execute("INSERT INTO user(name) VALUES(:Name)", dee);
    CHECK(!wrong_case && wrong_case.error().primary() == primary_code::range);

    expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM user").first();
    CHECK(count && *count == 3);
}

void test_kept_columns()
{
    database db = open_users();

    // Many rows through one lookup: the columns are kept with the statement
    for (int i = 0; i < 200; ++i)
    {
        CHECK(
            db.execute("INSERT INTO user(name, age, note) VALUES(?, ?, 'n')", "user" + std::to_string(i), i).has_value()
        );
    }
    std::int64_t age_sum = 0;
    int rows = 0;
    for (expected<app::user>& u : db.query<app::user>("SELECT note, id, age, name FROM user WHERE age IS NOT NULL"))
    {
        CHECK(u.has_value());
        if (u)
        {
            age_sum += *u->age;
            ++rows;
        }
    }
    CHECK(rows == 202 && age_sum == 31 + 25 + 199 * 200 / 2);

    // A schema change that moves the columns: the cached statement is prepared again, and so is the lookup
    CHECK(db.execute("CREATE TABLE person(extra TEXT, name TEXT, age INTEGER, id INTEGER)").has_value());
    CHECK(db.execute("INSERT INTO person VALUES('x', 'Ann', 30, 1)").has_value());
    expected<app::user> before = db.query<app::user>("SELECT * FROM person").first();
    CHECK(before && before->name == "Ann" && before->age && *before->age == 30 && before->id == 1);
    CHECK(db.execute("ALTER TABLE person DROP COLUMN extra").has_value());
    CHECK(db.execute("ALTER TABLE person ADD COLUMN later TEXT").has_value());
    expected<app::user> after = db.query<app::user>("SELECT * FROM person").first();
    CHECK(after && after->name == "Ann" && after->age && *after->age == 30 && after->id == 1);

    // Two records from one statement, one after the other
    expected<statement> select = db.prepare("SELECT name, age FROM user WHERE id = ?", 1);
    CHECK(select.has_value());
    if (select)
    {
        expected<app::renamed> as_renamed = select->first<app::renamed>();
        CHECK(as_renamed && as_renamed->who == "Alice" && as_renamed->years == 31);
        CHECK(select->reset().has_value());
        expected<app::name_view> as_view = select->first<app::name_view>();
        CHECK(as_view && as_view->name == "Alice");
        CHECK(select->reset().has_value());
        expected<app::renamed> again = select->first<app::renamed>();
        CHECK(again && again->who == "Alice");
    }

    // A missing column stays an error on every row, the kept columns or not
    int errors = 0;
    for (expected<counted>& c : db.query<counted>("SELECT id FROM user LIMIT 3"))
    {
        CHECK(!c && c.error().primary() == primary_code::range);
        ++errors;
    }
    CHECK(errors == 3);
}

void test_native_status()
{
    database db = open_users();
    expected<statement> select = db.prepare("SELECT * FROM user");
    CHECK(select.has_value());
    if (!select)
    {
        return;
    }
    sqlnx::sqlite::native::statement& stmt = select->native_statement();
    CHECK(stmt.status(sqlnx::sqlite::native::statement_status::reprepare) == 0);
    CHECK(select->first<app::user>().has_value());
    CHECK(select->reset().has_value());
    CHECK(db.execute("ALTER TABLE user ADD COLUMN later TEXT").has_value());
    CHECK(select->first<app::user>().has_value());
    CHECK(stmt.status(sqlnx::sqlite::native::statement_status::reprepare) == 1);
    CHECK(stmt.status(sqlnx::sqlite::native::statement_status::run, true) >= 1);
    CHECK(stmt.status(sqlnx::sqlite::native::statement_status::run) == 0);
}

void test_record_binder()
{
    database db = open_users();
    CHECK(db.execute("CREATE TABLE shape(x REAL, y REAL)").has_value());

    // By name and by index
    CHECK(db.execute("INSERT INTO shape(x, y) VALUES(:x, :y)", point(1.5, 2.5)).has_value());
    expected<point> read = db.query<point>("SELECT x, y FROM shape").first();
    CHECK(read && read->x == 1.5 && read->y == 2.5);

    // A parameter that got no value fails done(); the first error wins
    expected<void> missing = db.execute("INSERT INTO shape(x, y) VALUES(:x, :y + :z)", point(1, 2));
    CHECK(!missing && missing.error().primary() == primary_code::range);
    expected<void> unknown = db.execute("INSERT INTO shape(x, y) VALUES(:a, :b)", point(1, 2));
    CHECK(!unknown && unknown.error().primary() == primary_code::range);

    expected<std::int64_t> count = db.query<std::int64_t>("SELECT count(*) FROM shape").first();
    CHECK(count && *count == 1);
}

} // anonymous

int main()
{
    test_query();
    test_hand_written_fields();
    test_errors();
    test_everywhere_a_row_is_read();
    test_record_traits();
    test_binding_fields();
    test_binding_errors();
    test_record_binder();
    test_kept_columns();
    test_native_status();

    return test::summary();
}
