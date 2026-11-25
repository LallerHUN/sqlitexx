// Tests for sqlitexx::registrations: a token that removes functions, aggregates and collations when it ends.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstdint>
#include <string>
#include <utility>

using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::registrations;
using sqlitexx::statement;
using sqlitexx::string_view;
using sqlitexx::native::function_flags;
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

database open_numbers()
{
    database db = open_memory();
    CHECK(db.execute(
                "CREATE TABLE n(g INTEGER, x INTEGER);"
                "INSERT INTO n VALUES(1, 1), (1, 2), (2, 3), (2, 4), (3, 5)"
    )
              .has_value());
    return db;
}

template <class T>
expected<T> select(
    database& db,
    const char* sql
)
{
    return db.prepare(sql).and_then(sqlitexx::first<T>());
}

// Counts how many callables ended: the copy SQLite calls, not the moved-from ones
class tracker
{
public:
    explicit tracker(int* ended_count)
        : ended(ended_count)
    {
    }

    tracker(tracker&& other) noexcept
        : ended(other.ended)
    {
        other.ended = nullptr;
    }

    tracker(const tracker& other)
        : ended(other.ended)
    {
    }

    tracker& operator=(const tracker&) = delete;
    tracker& operator=(tracker&&) = delete;

    ~tracker()
    {
        if (ended != nullptr)
        {
            ++*ended;
        }
    }

private:
    int* ended;
};

// A function multiplying by a factor outside it
struct tracked : tracker
{
    const int* factor;

    tracked(
        int* ended_count,
        const int* factor_of
    )
        : tracker(ended_count),
          factor(factor_of)
    {
    }

    int operator()(int value) const
    {
        return value * *factor;
    }
};

// A collation in the direction of a factor outside it
struct tracked_order : tracker
{
    const int* factor;

    tracked_order(
        int* ended_count,
        const int* factor_of
    )
        : tracker(ended_count),
          factor(factor_of)
    {
    }

    int operator()(
        string_view first,
        string_view second
    ) const
    {
        return *factor * first.compare(second);
    }
};

// An aggregate summing its rows times a factor outside it (no member initializers: an aggregate in C++11 too)
struct scaled_sum
{
    const int* factor;
    std::int64_t sum;

    void step(int value)
    {
        sum += value * *factor;
    }

    std::int64_t finish() const
    {
        return sum;
    }
};

void test_scope_removes()
{
    int ended = 0;
    database db = open_numbers();
    {
        int factor = 10;
        registrations scope;
        CHECK(scope.empty());
        CHECK(db.create_function("scaled", tracked(&ended, &factor), scope).has_value());
        CHECK(db.create_function("scaled_fixed", tracked(&ended, &factor), function_flags::deterministic, scope)
                  .has_value());
        CHECK(db.create_aggregate("scaled_sum", scaled_sum{&factor, 0}, scope).has_value());
        CHECK(db.create_collation("scaled_order", tracked_order(&ended, &factor), scope).has_value());
        CHECK(!scope.empty());

        expected<int> scaled = select<int>(db, "SELECT scaled(4)");
        CHECK(scaled && *scaled == 40);
        factor = 2;
        expected<int> fixed = select<int>(db, "SELECT scaled_fixed(4)");
        CHECK(fixed && *fixed == 8);
        expected<std::int64_t> sum = select<std::int64_t>(db, "SELECT scaled_sum(x) FROM n");
        CHECK(sum && *sum == 30);
        factor = -1;
        expected<int> last = select<int>(db, "SELECT x FROM n ORDER BY CAST(x AS TEXT) COLLATE scaled_order LIMIT 1");
        CHECK(last && *last == 5);
        CHECK(ended == 0);
    }
    // The token ended first: the callables went with it, and SQLite has nothing left referring to factor
    CHECK(ended == 3);
    CHECK(!select<int>(db, "SELECT scaled(4)"));
    CHECK(!select<int>(db, "SELECT scaled_fixed(4)"));
    CHECK(!select<std::int64_t>(db, "SELECT scaled_sum(x) FROM n"));
    CHECK(!db.prepare("SELECT x FROM n ORDER BY CAST(x AS TEXT) COLLATE scaled_order"));
}

void test_remove_and_release()
{
    int ended = 0;
    int factor = 3;
    database db = open_memory();
    {
        registrations scope;
        CHECK(db.create_function("scaled", tracked(&ended, &factor), scope).has_value());
        CHECK(scope.remove().has_value());
        CHECK(scope.empty());
        CHECK(ended == 1);
        CHECK(!select<int>(db, "SELECT scaled(1)"));
        // Removing again, and ending, have nothing left to do
        CHECK(scope.remove().has_value());

        // release() keeps the registration past the token
        CHECK(db.create_function("kept", tracked(&ended, &factor), scope).has_value());
        scope.release();
        CHECK(scope.empty());
    }
    CHECK(ended == 1);
    expected<int> kept = select<int>(db, "SELECT kept(2)");
    CHECK(kept && *kept == 6);
    // It goes like one registered without a token: with remove_function, or when the database closes
    CHECK(db.remove_function("kept", 1).has_value());
    CHECK(ended == 2);
}

void test_gone_already()
{
    int ended = 0;
    int factor = 1;
    registrations outliving;
    {
        database db = open_memory();
        registrations scope;
        CHECK(db.create_function("replaced", tracked(&ended, &factor), scope).has_value());
        CHECK(db.create_function("removed", tracked(&ended, &factor), scope).has_value());
        CHECK(db.create_collation("collated", tracked_order(&ended, &factor), scope).has_value());

        // Replaced without the token: the token must not remove the new one
        CHECK(db.create_function(
                    "replaced",
                    [](int value)
                    {
                        return value + 100;
                    }
        ).has_value());
        CHECK(ended == 1);
        CHECK(db.remove_function("removed", 1).has_value());
        CHECK(ended == 2);
        CHECK(scope.remove().has_value());
        CHECK(ended == 3);
        expected<int> replaced = select<int>(db, "SELECT replaced(1)");
        CHECK(replaced && *replaced == 101);

        // A token outliving the database
        CHECK(db.create_function("closed_first", tracked(&ended, &factor), outliving).has_value());
    }
    // Closing released it; the token has nothing left to remove
    CHECK(ended == 4);
    CHECK(outliving.remove().has_value());
    CHECK(outliving.empty());
}

// Removing by name what a token holds: the callable ends at once, the token finds nothing left, and what is
// registered again under the name afterwards stays when the token ends
void test_removed_by_name()
{
    int ended = 0;
    int factor = 1;
    database db = open_memory();
    {
        registrations scope;
        CHECK(db.create_function("scaled", tracked(&ended, &factor), scope).has_value());
        CHECK(db.create_collation("ordered", tracked_order(&ended, &factor), scope).has_value());

        // Another number of parameters is another function: the token's one stays
        CHECK(db.remove_function("scaled", 2).has_value());
        CHECK(ended == 0);
        CHECK(select<int>(db, "SELECT scaled(1)").has_value());

        CHECK(db.remove_function("scaled", 1).has_value());
        CHECK(db.remove_collation("ordered").has_value());
        CHECK(ended == 2);
        CHECK(db.create_function(
                    "scaled",
                    [](int value)
                    {
                        return value + 100;
                    }
        ).has_value());
        CHECK(db.create_collation(
                    "ordered",
                    [](string_view first, string_view second)
                    {
                        return second.compare(first);
                    }
        ).has_value());
    }
    CHECK(ended == 2);
    expected<int> scaled = select<int>(db, "SELECT scaled(1)");
    CHECK(scaled && *scaled == 101);
    expected<std::string> first =
        select<std::string>(db, "SELECT x FROM (SELECT 'a' x UNION ALL SELECT 'b') ORDER BY x COLLATE ordered LIMIT 1");
    CHECK(first && *first == "b");
}

void test_busy()
{
    int ended = 0;
    int factor = 2;
    database db = open_numbers();
    registrations scope;
    CHECK(db.create_function("scaled", tracked(&ended, &factor), scope).has_value());

    // A statement that returned a row and wasn't reset keeps everything registered
    expected<statement> running = db.prepare("SELECT scaled(x) FROM n ORDER BY x");
    CHECK(running.has_value());
    expected<bool> row = running->step();
    CHECK(row && *row);
    expected<void> refused = scope.remove();
    CHECK(!refused && refused.error().primary() == primary_code::busy);
    CHECK(!scope.empty());
    CHECK(ended == 0);

    CHECK(running->reset().has_value());
    CHECK(scope.remove().has_value());
    CHECK(ended == 1);
}

void test_ended_while_running()
{
    int ended = 0;
    int factor = 2;
    database db = open_numbers();
    statement functions;
    statement aggregates;
    {
        registrations scope;
        CHECK(db.create_function("scaled", tracked(&ended, &factor), scope).has_value());
        CHECK(db.create_aggregate("scaled_sum", scaled_sum{&factor, 0}, scope).has_value());
        expected<statement> prepared = db.prepare("SELECT scaled(x) FROM n");
        CHECK(prepared.has_value());
        functions = std::move(*prepared);
        prepared = db.prepare("SELECT scaled_sum(x) FROM n GROUP BY g ORDER BY g");
        CHECK(prepared.has_value());
        aggregates = std::move(*prepared);

        // One row each: the function ran for the first row, the aggregate for the first group
        expected<bool> row = functions.step();
        CHECK(row && *row);
        expected<int> first = functions.column<int>(0);
        CHECK(first && *first == 2);
        row = aggregates.step();
        CHECK(row && *row);
        expected<std::int64_t> group = aggregates.column<std::int64_t>(0);
        CHECK(group && *group == 6);
    }
    // The token couldn't remove them: it ended the callables, and the calls still to come fail
    CHECK(ended == 1);
    expected<bool> next = functions.step();
    CHECK(!next && next.error().primary() == primary_code::misuse);
    expected<bool> next_group = aggregates.step();
    CHECK(!next_group && next_group.error().primary() == primary_code::misuse);

    // Once the statements are done, the names can be registered again
    static_cast<void>(functions.reset());
    static_cast<void>(aggregates.reset());
    CHECK(db.create_function(
                "scaled",
                [](int value)
                {
                    return value;
                }
    ).has_value());
    expected<int> again = select<int>(db, "SELECT scaled(7)");
    CHECK(again && *again == 7);
}

void test_move()
{
    int ended = 0;
    int factor = 1;
    database db = open_memory();
    registrations moved_to;
    {
        registrations scope;
        CHECK(db.create_function("first", tracked(&ended, &factor), scope).has_value());
        moved_to = std::move(scope);
        CHECK(scope.empty());
    }
    CHECK(ended == 0);
    CHECK(select<int>(db, "SELECT first(1)").has_value());

    // Move-assigning ends what the target held
    registrations other;
    CHECK(db.create_function("second", tracked(&ended, &factor), other).has_value());
    moved_to = std::move(other);
    CHECK(ended == 1);
    CHECK(!select<int>(db, "SELECT first(1)"));

    registrations constructed(std::move(moved_to));
    CHECK(moved_to.empty());
    CHECK(constructed.remove().has_value());
    CHECK(ended == 2);
}

void test_several_databases()
{
    int ended = 0;
    int factor = 5;
    database one = open_memory();
    database two = open_memory();
    {
        registrations scope;
        CHECK(one.create_function("scaled", tracked(&ended, &factor), scope).has_value());
        CHECK(two.create_function("scaled", tracked(&ended, &factor), scope).has_value());
        expected<int> from_two = select<int>(two, "SELECT scaled(2)");
        CHECK(from_two && *from_two == 10);
    }
    CHECK(ended == 2);
    CHECK(!select<int>(one, "SELECT scaled(2)"));
    CHECK(!select<int>(two, "SELECT scaled(2)"));
}

void test_failing_registration()
{
    int ended = 0;
    int factor = 1;
    database db = open_memory();
    registrations scope;

    // SQLite rejects names over 255 bytes, and releases what it was given
    const std::string long_name(300, 'f');
    expected<void> rejected = db.create_function(long_name.c_str(), tracked(&ended, &factor), scope);
    CHECK(!rejected && rejected.error().primary() == primary_code::misuse);
    CHECK(ended == 1);
    CHECK(scope.empty());

    // A collation can't be replaced while a statement runs; the token doesn't keep the rejected one
    CHECK(db.create_collation("ordered", tracked_order(&ended, &factor)).has_value());
    expected<statement> running =
        db.prepare("SELECT x FROM (SELECT 'a' x UNION ALL SELECT 'b') ORDER BY x COLLATE ordered");
    CHECK(running.has_value());
    expected<bool> row = running->step();
    CHECK(row && *row);
    expected<void> busy = db.create_collation("ordered", tracked_order(&ended, &factor), scope);
    CHECK(!busy && busy.error().primary() == primary_code::busy);
    CHECK(ended == 2);
    CHECK(scope.empty());

    database closed;
    expected<void> not_open = closed.create_collation("ordered", tracked_order(&ended, &factor), scope);
    CHECK(!not_open && not_open.error().primary() == primary_code::misuse);
    CHECK(scope.empty());
}
} // anonymous

int main()
{
    test_scope_removes();
    test_remove_and_release();
    test_gone_already();
    test_removed_by_name();
    test_busy();
    test_ended_while_running();
    test_move();
    test_several_databases();
    test_failing_registration();
    return test::summary();
}
