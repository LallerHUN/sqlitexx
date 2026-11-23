// Tests for database::create_function: SQL functions written in C++.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#include <stdexcept>
#define SQLITEXX_TEST_EXCEPTIONS 1
#else
#define SQLITEXX_TEST_EXCEPTIONS 0
#endif

using sqlitexx::blob;
using sqlitexx::cstring_view;
using sqlitexx::database;
using sqlitexx::error;
using sqlitexx::expected;
using sqlitexx::nullopt;
using sqlitexx::optional;
using sqlitexx::unexpect;
using sqlitexx::native::extended_code;
using sqlitexx::native::function_flags;
using sqlitexx::native::primary_code;

// Own types: one with traits written as templates, which work for statements and SQL functions alike, and one with
// traits written for native::statement only, as before SQL functions existed
struct celsius
{
    double degrees = 0;
};

struct label
{
    std::string text;
};

namespace sqlitexx
{
template <>
struct value_traits<celsius>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const celsius& value
    )
    {
        return stmt.bind_double(index, value.degrees);
    }

    template <class Source>
    static expected<celsius> read(
        const Source& stmt,
        int index
    )
    {
        celsius value;
        value.degrees = stmt.column_double(index);
        return value;
    }
};

template <>
struct value_traits<label>
{
    static native::result bind(
        native::statement& stmt,
        int index,
        const label& value
    )
    {
        return stmt.bind_text(index, string_view(value.text));
    }

    static expected<label> read(
        const native::statement& stmt,
        int index
    )
    {
        label value;
        value.text = std::string(stmt.column_text(index)->c_str());
        return value;
    }
};
} // sqlitexx

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

// The single value a query gives, or its error
template <class T>
expected<T> select(
    database& db,
    const char* sql
)
{
    return db.prepare(sql).and_then(sqlitexx::first<T>());
}

int twice(int value)
{
    return value * 2;
}

void test_numbers()
{
    database db = open_memory();
    CHECK(db.create_function(
                "plus",
                [](int left, int right)
                {
                    return left + right;
                }
    ).has_value());
    CHECK(db.create_function("twice", &twice).has_value());
    CHECK(db.create_function(
                "half",
                [](double value)
                {
                    return value / 2;
                }
    ).has_value());

    expected<int> sum = select<int>(db, "SELECT plus(2, 3)");
    CHECK(sum && *sum == 5);
    expected<int> doubled = select<int>(db, "SELECT twice(plus(1, 1))");
    CHECK(doubled && *doubled == 4);
    expected<double> halved = select<double>(db, "SELECT half(3)");
    CHECK(halved && *halved == 1.5);

    // The result has SQLite's type for the C++ type
    expected<std::string> type = select<std::string>(db, "SELECT typeof(plus(1, 2)) || typeof(half(1))");
    CHECK(type && *type == "integerreal");
}

void test_text_and_blobs()
{
    database db = open_memory();
    CHECK(db.create_function(
                "greet",
                [](const std::string& name)
                {
                    return "Hello, " + name;
                }
    ).has_value());
    // A view into the argument as the result: SQLite copies it before the argument goes away
    CHECK(db.create_function(
                "same",
                [](cstring_view text)
                {
                    return text;
                }
    ).has_value());
    CHECK(db.create_function(
                "size",
                [](const blob& data)
                {
                    return data.size();
                }
    ).has_value());
    CHECK(db.create_function(
                "bytes",
                [](int count)
                {
                    return blob(static_cast<std::size_t>(count), 0xAB);
                }
    ).has_value());

    expected<std::string> greeting = select<std::string>(db, "SELECT greet('Alice')");
    CHECK(greeting && *greeting == "Hello, Alice");
    expected<std::string> same = select<std::string>(db, "SELECT same('as is') || same('')");
    CHECK(same && *same == "as is");
    expected<std::int64_t> size = select<std::int64_t>(db, "SELECT size(x'00010203')");
    CHECK(size && *size == 4);
    expected<blob> made = select<blob>(db, "SELECT bytes(3)");
    CHECK(made && *made == blob(3, 0xAB));

    // An empty blob stays a blob
    expected<std::string> empty = select<std::string>(db, "SELECT typeof(bytes(0))");
    CHECK(empty && *empty == "blob");
}

void test_null()
{
    database db = open_memory();
    CHECK(db.create_function(
                "double_or_null",
                [](optional<int> value)
                {
                    return value ? optional<int>(*value * 2) : optional<int>();
                }
    ).has_value());
    CHECK(db.create_function(
                "strict",
                [](int value)
                {
                    return value;
                }
    ).has_value());

    expected<optional<int>> doubled = select<optional<int>>(db, "SELECT double_or_null(21)");
    CHECK(doubled && *doubled && **doubled == 42);
    expected<optional<int>> null = select<optional<int>>(db, "SELECT double_or_null(NULL)");
    CHECK(null && !*null);

    // NULL into a parameter that is not an optional fails the call
    expected<int> refused = select<int>(db, "SELECT strict(NULL)");
    CHECK(!refused && refused.error().primary() == primary_code::mismatch);
    CHECK(!refused && refused.error().message() == "argument 1 is NULL; take it as an optional to accept NULL");

    // So does an integer that doesn't fit the parameter
    expected<int> too_big = select<int>(db, "SELECT strict(9223372036854775807)");
    CHECK(!too_big && too_big.error().primary() == primary_code::mismatch);
    CHECK(!too_big && too_big.error().message() == "argument 1: the integer does not fit the requested type");
}

void test_errors()
{
    database db = open_memory();
    CHECK(db.create_function(
                "checked_sqrt",
                [](double value)
                {
                    if (value < 0)
                    {
                        return expected<double>(
                            unexpect,
                            error(
                                sqlitexx::native::result(static_cast<int>(extended_code::constraint_check)),
                                "checked_sqrt: negative argument"
                            )
                        );
                    }
                    return expected<double>(value < 1 ? value : 2.0);
                }
    ).has_value());

    expected<double> fine = select<double>(db, "SELECT checked_sqrt(4)");
    CHECK(fine && *fine == 2.0);

    // The statement fails with the function's code and message
    expected<double> failed = select<double>(db, "SELECT checked_sqrt(-1)");
    CHECK(!failed);
    CHECK(!failed && failed.error().code() == extended_code::constraint_check);
    CHECK(!failed && failed.error().message() == "checked_sqrt: negative argument");

    // A result that doesn't fit SQLite's integers is an error, not a wrapped value
    CHECK(db.create_function(
                "huge",
                []
                {
                    return static_cast<std::uint64_t>(-1);
                }
    ).has_value());
    expected<std::int64_t> huge = select<std::int64_t>(db, "SELECT huge()");
    CHECK(!huge && huge.error().primary() == primary_code::mismatch);

    // SQLite checks the number of arguments when preparing
    expected<int> wrong_count = select<int>(db, "SELECT checked_sqrt(1, 2)");
    CHECK(!wrong_count && wrong_count.error().primary() == primary_code::error);

    // No database, no function
    database closed;
    expected<void> nowhere = closed.create_function(
        "f",
        []
        {
            return 1;
        }
    );
    CHECK(!nowhere && nowhere.error().primary() == primary_code::misuse);
}

void test_void_state_and_lifetime()
{
    std::shared_ptr<int> calls = std::make_shared<int>(0);
    {
        database db = open_memory();

        // void returns NULL; the copy SQLite keeps holds the state
        CHECK(db.create_function(
                    "touch",
                    [calls]
                    {
                        ++*calls;
                    }
        ).has_value());
        CHECK(calls.use_count() == 2);
        expected<optional<int>> touched = select<optional<int>>(db, "SELECT touch()");
        CHECK(touched && !*touched);
        CHECK(db.execute("SELECT touch(); SELECT touch();").has_value());
        CHECK(*calls == 3);

        // A mutable lambda keeps its own state between calls
        int counter = 0;
        CHECK(db.create_function(
                    "next",
                    [counter]() mutable
                    {
                        return ++counter;
                    }
        ).has_value());
        expected<int> first = select<int>(db, "SELECT next()");
        CHECK(first && *first == 1);
        expected<int> second = select<int>(db, "SELECT next()");
        CHECK(second && *second == 2);

        // Replacing a function releases the old copy
        CHECK(db.create_function(
                    "touch",
                    []
                    {
                        return 0;
                    }
        ).has_value());
        CHECK(calls.use_count() == 1);

        CHECK(db.create_function(
                    "touch_again",
                    [calls]
                    {
                        return *calls;
                    }
        ).has_value());
        CHECK(calls.use_count() == 2);
    }
    // Closing the database releases the rest
    CHECK(calls.use_count() == 1);
}

void test_own_types()
{
    database db = open_memory();

    // Template traits: arguments and results of SQL functions
    CHECK(db.create_function(
                "warmer",
                [](celsius value, double by)
                {
                    celsius result;
                    result.degrees = value.degrees + by;
                    return result;
                }
    ).has_value());
    expected<celsius> warmer = select<celsius>(db, "SELECT warmer(20.5, 1)");
    CHECK(warmer && warmer->degrees == 21.5);

    // Traits for native::statement still work with statements
    label tag;
    tag.text = "kept";
    expected<label> echoed =
        db.prepare("SELECT ?").and_then(sqlitexx::bind_values(tag)).and_then(sqlitexx::first<label>());
    CHECK(echoed && echoed->text == "kept");
}

void test_deterministic()
{
    database db = open_memory();
    CHECK(db.execute("CREATE TABLE item(name TEXT)").has_value());
    CHECK(db.create_function(
                "anything",
                [](const std::string& text)
                {
                    return text;
                }
    ).has_value());
    CHECK(db.create_function(
                "lower_ascii",
                [](std::string text)
                {
                    for (char& c : text)
                    {
                        if (c >= 'A' && c <= 'Z')
                        {
                            c = static_cast<char>(c - 'A' + 'a');
                        }
                    }
                    return text;
                },
                function_flags::deterministic | function_flags::innocuous
    )
              .has_value());

    // Only deterministic functions may be used in an index
    CHECK(!db.execute("CREATE INDEX item_any ON item(anything(name))").has_value());
    CHECK(db.execute("CREATE INDEX item_lower ON item(lower_ascii(name))").has_value());
    CHECK(db.execute("INSERT INTO item VALUES('MiXeD')").has_value());
    expected<std::string> found = select<std::string>(db, "SELECT name FROM item WHERE lower_ascii(name) = 'mixed'");
    CHECK(found && *found == "MiXeD");
}

#if SQLITEXX_TEST_EXCEPTIONS
void test_exceptions()
{
    database db = open_memory();
    CHECK(db.create_function(
                "boom",
                []() -> int
                {
                    throw std::runtime_error("boom went the function");
                }
    ).has_value());
    expected<int> failed = select<int>(db, "SELECT boom()");
    CHECK(!failed && failed.error().primary() == primary_code::error);
    CHECK(!failed && failed.error().message() == "boom went the function");
}
#endif
} // anonymous

int main()
{
    test_numbers();
    test_text_and_blobs();
    test_null();
    test_errors();
    test_void_state_and_lifetime();
    test_own_types();
    test_deterministic();
#if SQLITEXX_TEST_EXCEPTIONS
    test_exceptions();
#endif
    return test::summary();
}
