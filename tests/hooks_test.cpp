// Tests for the hooks: commit_hook, rollback_hook, update_hook and progress_handler.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#include <stdexcept>
#define SQLITEXX_TEST_EXCEPTIONS 1
#else
#define SQLITEXX_TEST_EXCEPTIONS 0
#endif

using sqlitexx::cstring_view;
using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::registrations;
using sqlitexx::native::extended_code;
using sqlitexx::native::primary_code;
using sqlitexx::native::update_kind;

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

database open_items()
{
    database db = open_memory();
    CHECK(db.execute("CREATE TABLE item(name TEXT)").has_value());
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

// A row change as an update hook reports it
struct change
{
    update_kind kind;
    std::string schema;
    std::string table;
    std::int64_t rowid;
};

// Counts how many hooks ended: the one the database keeps, not the moved-from ones
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

    tracker(const tracker&) = delete;
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

// A commit hook counting commits, saying yes
struct counting_commits : tracker
{
    int* commits;

    counting_commits(
        int* ended_count,
        int* commit_count
    )
        : tracker(ended_count),
          commits(commit_count)
    {
    }

    bool operator()() const
    {
        ++*commits;
        return true;
    }
};

void test_update_hook()
{
    database db = open_items();
    std::vector<change> changes;
    CHECK(db.update_hook(
                [&changes](update_kind kind, cstring_view schema, cstring_view table, std::int64_t rowid)
                {
                    changes.push_back(change{kind, std::string(schema.c_str()), std::string(table.c_str()), rowid});
                }
    ).has_value());

    CHECK(db.execute(
                "INSERT INTO item VALUES('a'), ('b');"
                "UPDATE item SET name = 'c' WHERE rowid = 2;"
                "DELETE FROM item WHERE rowid = 1"
    )
              .has_value());
    CHECK(changes.size() == 4);
    if (changes.size() == 4)
    {
        CHECK(changes[0].kind == update_kind::insert && changes[0].rowid == 1);
        CHECK(changes[1].kind == update_kind::insert && changes[1].rowid == 2);
        CHECK(changes[2].kind == update_kind::update && changes[2].rowid == 2);
        CHECK(changes[3].kind == update_kind::remove && changes[3].rowid == 1);
        CHECK(changes[0].schema == "main" && changes[0].table == "item");
    }

    // Cleared, it sees nothing
    CHECK(db.update_hook(nullptr).has_value());
    CHECK(db.execute("INSERT INTO item VALUES('d')").has_value());
    CHECK(changes.size() == 4);
}

void test_commit_hook()
{
    database db = open_items();
    int commits = 0;
    CHECK(db.commit_hook(
                [&commits]
                {
                    ++commits;
                }
    ).has_value());
    // Every statement outside a transaction commits on its own; a transaction commits once
    CHECK(db.execute("INSERT INTO item VALUES('a'); INSERT INTO item VALUES('b')").has_value());
    CHECK(db.execute("BEGIN; INSERT INTO item VALUES('c'); INSERT INTO item VALUES('d'); COMMIT").has_value());
    CHECK(commits == 3);

    // false turns the commit into a rollback
    bool allow = false;
    CHECK(db.commit_hook(
                [&allow]
                {
                    return allow;
                }
    ).has_value());
    expected<void> refused = db.execute("INSERT INTO item VALUES('e')");
    CHECK(!refused && refused.error().code() == extended_code::constraint_commithook);
    expected<int> count = select<int>(db, "SELECT count(*) FROM item");
    CHECK(count && *count == 4);
    allow = true;
    CHECK(db.execute("INSERT INTO item VALUES('e')").has_value());
}

void test_rollback_hook()
{
    database db = open_items();
    int rollbacks = 0;
    CHECK(db.rollback_hook(
                [&rollbacks]
                {
                    ++rollbacks;
                }
    ).has_value());
    CHECK(db.execute("BEGIN; INSERT INTO item VALUES('a'); ROLLBACK").has_value());
    CHECK(rollbacks == 1);
    CHECK(db.execute("BEGIN; INSERT INTO item VALUES('a'); COMMIT").has_value());
    CHECK(rollbacks == 1);
}

// A query that runs long enough to call the progress handler many times
const char* const counting = "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 100000)"
                             " SELECT count(*) FROM n";

void test_progress_handler()
{
    database db = open_memory();
    int calls = 0;
    CHECK(db.progress_handler(
                100,
                [&calls]
                {
                    ++calls;
                    return true;
                }
    ).has_value());
    expected<int> all = select<int>(db, counting);
    CHECK(all && *all == 100000);
    CHECK(calls > 10);

    // false interrupts
    calls = 0;
    CHECK(db.progress_handler(
                100,
                [&calls]
                {
                    return ++calls < 5;
                }
    ).has_value());
    expected<int> stopped = select<int>(db, counting);
    CHECK(!stopped && stopped.error().primary() == primary_code::interrupt);
    CHECK(calls == 5);

    CHECK(db.progress_handler(nullptr).has_value());
    calls = 0;
    CHECK(select<int>(db, counting).has_value());
    CHECK(calls == 0);

    expected<void> none = db.progress_handler(
        0,
        []
        {
            return true;
        }
    );
    CHECK(!none && none.error().primary() == primary_code::range);
}

void test_replace_and_release()
{
    int ended = 0;
    int commits = 0;
    {
        database db = open_items();
        CHECK(db.commit_hook(counting_commits(&ended, &commits)).has_value());
        CHECK(db.execute("INSERT INTO item VALUES('a')").has_value());
        CHECK(commits == 1);

        // Replacing releases the one before at once
        CHECK(db.commit_hook(counting_commits(&ended, &commits)).has_value());
        CHECK(ended == 1);
        // Clearing does too
        CHECK(db.commit_hook(nullptr).has_value());
        CHECK(ended == 2);
        CHECK(db.execute("INSERT INTO item VALUES('b')").has_value());
        CHECK(commits == 1);

        CHECK(db.commit_hook(counting_commits(&ended, &commits)).has_value());
    }
    // Closing releases the last one
    CHECK(ended == 3);

    database closed;
    expected<void> not_open = closed.commit_hook(counting_commits(&ended, &commits));
    CHECK(!not_open && not_open.error().primary() == primary_code::misuse);
    expected<void> not_cleared = closed.commit_hook(nullptr);
    CHECK(!not_cleared && not_cleared.error().primary() == primary_code::misuse);
}

void test_moving_the_database()
{
    int ended = 0;
    int commits = 0;
    database moved_to;
    {
        database db = open_items();
        CHECK(db.commit_hook(counting_commits(&ended, &commits)).has_value());
        database constructed(std::move(db));
        CHECK(constructed.execute("INSERT INTO item VALUES('a')").has_value());
        CHECK(commits == 1);
        moved_to = std::move(constructed);
    }
    CHECK(ended == 0);
    CHECK(moved_to.execute("INSERT INTO item VALUES('b')").has_value());
    CHECK(commits == 2);

    // Assigning over a database releases its hooks before its connection closes
    moved_to = open_items();
    CHECK(ended == 1);
    CHECK(moved_to.execute("INSERT INTO item VALUES('c')").has_value());
    CHECK(commits == 2);
}

void test_registrations()
{
    int ended = 0;
    int commits = 0;
    database db = open_items();
    {
        registrations scope;
        std::vector<std::int64_t> rows;
        CHECK(db.update_hook(
                    [&rows](update_kind, cstring_view, cstring_view, std::int64_t rowid)
                    {
                        rows.push_back(rowid);
                    },
                    scope
        )
                  .has_value());
        CHECK(db.commit_hook(counting_commits(&ended, &commits), scope).has_value());
        CHECK(db.rollback_hook(
                    [&rows]
                    {
                        rows.clear();
                    },
                    scope
        )
                  .has_value());
        CHECK(db.progress_handler(
                    1000,
                    [&rows]
                    {
                        return rows.size() < 1000;
                    },
                    scope
        )
                  .has_value());

        CHECK(db.execute("INSERT INTO item VALUES('a')").has_value());
        CHECK(rows.size() == 1 && commits == 1);
        CHECK(db.execute("BEGIN; INSERT INTO item VALUES('b'); ROLLBACK").has_value());
        CHECK(rows.empty());
    }
    // The token cleared them all: nothing refers to rows any more
    CHECK(ended == 1);
    CHECK(db.execute("BEGIN; INSERT INTO item VALUES('c'); ROLLBACK").has_value());
    CHECK(db.execute("INSERT INTO item VALUES('d')").has_value());
    CHECK(commits == 1);

    // A hook replaced after the token took it is left alone when the token ends
    {
        registrations scope;
        CHECK(db.commit_hook(counting_commits(&ended, &commits), scope).has_value());
        CHECK(db.commit_hook(counting_commits(&ended, &commits)).has_value());
        CHECK(ended == 2);
    }
    CHECK(ended == 2);
    CHECK(db.execute("INSERT INTO item VALUES('e')").has_value());
    CHECK(commits == 2);

    // A token clears hooks also while a statement runs
    {
        registrations scope;
        CHECK(db.update_hook([](update_kind, cstring_view, cstring_view, std::int64_t) {}, scope).has_value());
        expected<sqlitexx::statement> running = db.prepare("SELECT name FROM item");
        CHECK(running.has_value());
        expected<bool> row = running->step();
        CHECK(row && *row);
        CHECK(scope.remove().has_value());
        CHECK(scope.empty());
    }

    // release() keeps the hook; the token may outlive the database
    registrations outliving;
    {
        registrations scope;
        CHECK(db.commit_hook(counting_commits(&ended, &commits), scope).has_value());
        scope.release();
    }
    CHECK(ended == 3);
    CHECK(db.execute("INSERT INTO item VALUES('f')").has_value());
    CHECK(commits == 3);
    {
        database short_lived = open_items();
        CHECK(short_lived.commit_hook(counting_commits(&ended, &commits), outliving).has_value());
    }
    CHECK(ended == 4);
    CHECK(outliving.remove().has_value());
}

#if SQLITEXX_TEST_EXCEPTIONS
void test_exceptions()
{
    database db = open_items();
    CHECK(db.commit_hook(
                []() -> bool
                {
                    throw std::runtime_error("no commits today");
                }
    ).has_value());
    expected<void> refused = db.execute("INSERT INTO item VALUES('a')");
    CHECK(!refused && refused.error().code() == extended_code::constraint_commithook);
    CHECK(db.commit_hook(nullptr).has_value());

    CHECK(db.progress_handler(
                100,
                []() -> bool
                {
                    throw std::runtime_error("enough");
                }
    ).has_value());
    expected<int> stopped = select<int>(db, counting);
    CHECK(!stopped && stopped.error().primary() == primary_code::interrupt);
}
#endif

// The native layer takes C callbacks and returns the previous user_data
int count_native(void* user_data)
{
    ++*static_cast<int*>(user_data);
    return 0;
}

void test_native()
{
    database db = open_items();
    sqlitexx::native::connection& native = db.native_connection();
    int first = 0;
    int second = 0;
    CHECK(native.commit_hook(&count_native, &first) == nullptr);
    CHECK(db.execute("INSERT INTO item VALUES('a')").has_value());
    CHECK(native.commit_hook(&count_native, &second) == &first);
    CHECK(db.execute("INSERT INTO item VALUES('b')").has_value());
    CHECK(first == 1 && second == 1);
    CHECK(native.commit_hook(nullptr, nullptr) == &second);
}

// What is and isn't a hook, at compile time
struct no_arguments
{
    void operator()() const;
};

struct says_yes
{
    bool operator()() const;
};

struct takes_an_int
{
    bool operator()(int) const;
};

struct sees_changes
{
    void operator()(
        update_kind,
        cstring_view,
        cstring_view,
        std::int64_t
    ) const;
};

// clang-format off
static_assert(sqlitexx::detail::is_yes_no_hook<no_arguments, true>::value, "a void commit hook");
static_assert(sqlitexx::detail::is_yes_no_hook<says_yes, true>::value, "a bool commit hook");
static_assert(!sqlitexx::detail::is_yes_no_hook<no_arguments, false>::value, "a progress handler must answer");
static_assert(sqlitexx::detail::is_yes_no_hook<says_yes, false>::value, "a progress handler");
static_assert(!sqlitexx::detail::is_yes_no_hook<takes_an_int, true>::value, "hooks take no arguments");
static_assert(sqlitexx::detail::is_hook_callable<sees_changes, sqlitexx::detail::update_hook_signature>::value, "an update hook");
static_assert(!sqlitexx::detail::is_hook_callable<no_arguments, sqlitexx::detail::update_hook_signature>::value, "not an update hook");
// clang-format on
} // anonymous

int main()
{
    test_update_hook();
    test_commit_hook();
    test_rollback_hook();
    test_progress_handler();
    test_replace_and_release();
    test_moving_the_database();
    test_registrations();
#if SQLITEXX_TEST_EXCEPTIONS
    test_exceptions();
#endif
    test_native();
    return test::summary();
}
