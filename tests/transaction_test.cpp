// Tests for transaction and in_transaction.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstdint>
#include <utility>

using sqlitexx::database;
using sqlitexx::error;
using sqlitexx::expected;
using sqlitexx::transaction;
using sqlitexx::transaction_mode;
using sqlitexx::native::primary_code;

namespace
{
database open_empty()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    CHECK(db->execute("CREATE TABLE item(id INTEGER PRIMARY KEY, name TEXT NOT NULL)").has_value());
    return std::move(*db);
}

std::int64_t count_items(database& db)
{
    return db.prepare("SELECT count(*) FROM item").and_then(sqlitexx::first<std::int64_t>()).value_or(-1);
}

bool in_autocommit(database& db)
{
    return db.native_connection().autocommit();
}

void test_commit_keeps_changes()
{
    database db = open_empty();
    expected<transaction> tx = transaction::begin(db);
    CHECK(tx.has_value());
    CHECK(tx && tx->active());
    CHECK(!in_autocommit(db));

    CHECK(db.execute("INSERT INTO item(name) VALUES('a'), ('b')").has_value());
    CHECK(tx && tx->commit().has_value());
    CHECK(tx && !tx->active());
    CHECK(in_autocommit(db));
    CHECK(count_items(db) == 2);

    // Ended transactions refuse a second end
    expected<void> again = tx->commit();
    CHECK(!again && again.error().primary() == primary_code::misuse);
}

void test_scope_exit_rolls_back()
{
    database db = open_empty();
    {
        expected<transaction> tx = transaction::begin(db, transaction_mode::immediate);
        CHECK(tx.has_value());
        CHECK(db.execute("INSERT INTO item(name) VALUES('a')").has_value());
        CHECK(count_items(db) == 1);
    }
    CHECK(in_autocommit(db));
    CHECK(count_items(db) == 0);
}

void test_explicit_rollback()
{
    database db = open_empty();
    expected<transaction> tx = transaction::begin(db, transaction_mode::exclusive);
    CHECK(tx.has_value());
    CHECK(db.execute("INSERT INTO item(name) VALUES('a')").has_value());
    CHECK(tx && tx->rollback().has_value());
    CHECK(tx && !tx->active());
    CHECK(count_items(db) == 0);
}

void test_nested_begin_fails()
{
    database db = open_empty();
    expected<transaction> outer = transaction::begin(db);
    CHECK(outer.has_value());
    expected<transaction> inner = transaction::begin(db);
    CHECK(!inner.has_value());
    CHECK(!inner && inner.error().message().find("within a transaction") != std::string::npos);
    CHECK(outer && outer->active()); // the failed BEGIN does not disturb the open transaction
}

void test_moves()
{
    database db = open_empty();
    expected<transaction> tx = transaction::begin(db);
    CHECK(tx.has_value());
    CHECK(db.execute("INSERT INTO item(name) VALUES('a')").has_value());

    transaction moved(std::move(*tx));
    CHECK(!tx->active());
    CHECK(moved.active());
    CHECK(moved.commit().has_value());
    CHECK(count_items(db) == 1);

    // Assigning over an open transaction rolls that one back
    expected<transaction> first = transaction::begin(db);
    CHECK(first.has_value());
    CHECK(db.execute("INSERT INTO item(name) VALUES('b')").has_value());
    transaction empty;
    *first = std::move(empty);
    CHECK(in_autocommit(db));
    CHECK(count_items(db) == 1);
}

void test_failed_commit_can_stay_open()
{
    // A deferred foreign key violation makes COMMIT fail but leaves the transaction open (SQLite's documented rule)
    database db = open_empty();
    CHECK(db.execute(
                "PRAGMA foreign_keys = ON;"
                "CREATE TABLE child(id INTEGER PRIMARY KEY, item_id INTEGER REFERENCES item(id) DEFERRABLE INITIALLY "
                "DEFERRED)"
    )
              .has_value());

    expected<transaction> tx = transaction::begin(db);
    CHECK(tx.has_value());
    CHECK(db.execute("INSERT INTO child(item_id) VALUES(42)").has_value());

    expected<void> committed = tx->commit();
    CHECK(!committed.has_value());
    CHECK(!committed && committed.error().primary() == primary_code::constraint);
    CHECK(tx->active());
    CHECK(!in_autocommit(db));

    // Fix the data and commit again
    CHECK(db.execute("INSERT INTO item(id, name) VALUES(42, 'parent')").has_value());
    CHECK(tx->commit().has_value());
    CHECK(!tx->active());
    CHECK(count_items(db) == 1);
}

void test_in_transaction()
{
    database db = open_empty();

    // Success commits and passes the value on
    expected<std::int64_t> id = sqlitexx::in_transaction(
        db,
        [&]
        {
            return db.prepare("INSERT INTO item(name) VALUES(?)")
                .and_then(sqlitexx::execute_with("a"))
                .transform(
                    [&]
                    {
                        return db.last_insert_rowid();
                    }
                );
        }
    );
    CHECK(id && *id == 1);
    CHECK(in_autocommit(db));
    CHECK(count_items(db) == 1);

    // An error rolls back everything the body did and is passed on unchanged
    expected<void> failed = sqlitexx::in_transaction(
        db,
        [&]
        {
            return db.execute("INSERT INTO item(name) VALUES('b')")
                .and_then(
                    [&]
                    {
                        return db.execute("INSERT INTO item(name) VALUES(NULL)");
                    }
                );
        }
    );
    CHECK(!failed && failed.error().primary() == primary_code::constraint);
    CHECK(in_autocommit(db));
    CHECK(count_items(db) == 1);

    // BEGIN failing is reported, and the body does not run
    expected<transaction> outer = transaction::begin(db);
    CHECK(outer.has_value());
    bool ran = false;
    expected<void> nested = sqlitexx::in_transaction(
        db,
        [&]
        {
            ran = true;
            return expected<void>();
        }
    );
    CHECK(!nested.has_value());
    CHECK(!ran);
}
} // anonymous

int main()
{
    test_commit_keeps_changes();
    test_scope_exit_rolls_back();
    test_explicit_rollback();
    test_nested_begin_fails();
    test_moves();
    test_failed_commit_can_stay_open();
    test_in_transaction();

    return test::summary();
}
