// Tests for the native layer.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <cstring>
#include <utility>

using namespace sqlitexx::native;
using sqlitexx::string_view;
using sqlitexx::u16string_view;

namespace
{
void test_open_and_exec()
{
    connection db;
    CHECK(!db);
    CHECK(db.open(":memory:").ok());
    CHECK(static_cast<bool>(db));
    CHECK(db.exec("CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT, age INTEGER, score REAL, data BLOB)").ok());
    CHECK(db.autocommit());
    CHECK(db.close().ok());
    CHECK(!db);
}

void test_failed_open_keeps_message()
{
    connection db;
    const result rc = db.open("this/directory/does/not/exist/db.sqlite", open_flags::readonly);
    CHECK(rc == primary_code::cantopen);
    CHECK(rc.failed());
    CHECK(std::strlen(db.errmsg()) > 0);
    CHECK(std::strlen(rc.errstr()) > 0);
}

void test_bind_step_and_columns()
{
    connection db;
    CHECK(db.open(":memory:").ok());
    CHECK(db.exec("CREATE TABLE user(id INTEGER PRIMARY KEY, name TEXT, age INTEGER, score REAL, data BLOB)").ok());

    statement insert;
    CHECK(db.prepare("INSERT INTO user(name, age, score, data) VALUES(:name, ?2, ?3, ?4)", insert).ok());
    CHECK(insert.bind_parameter_count() == 4);
    CHECK(insert.bind_parameter_index(":name") == 1);
    CHECK(!insert.readonly());

    const unsigned char bytes[] = {1, 2, 3};
    CHECK(insert.bind_text(1, "Alice").ok());
    CHECK(insert.bind_int(2, 31).ok());
    CHECK(insert.bind_double(3, 1.5).ok());
    CHECK(insert.bind_blob(4, bytes, sizeof(bytes)).ok());
    CHECK(insert.step().done());
    CHECK(db.last_insert_rowid() == 1);
    CHECK(db.changes() == 1);

    // An empty string_view must stay an empty text, not become NULL
    CHECK(insert.reset().ok());
    CHECK(insert.clear_bindings().ok());
    CHECK(insert.bind_text(1, string_view()).ok());
    CHECK(insert.bind_int64(2, 5000000000LL).ok());
    CHECK(insert.step().done());

    statement select;
    CHECK(db.prepare("SELECT name, age, score, data FROM user ORDER BY id", select).ok());
    CHECK(select.readonly());
    CHECK(select.column_count() == 4);
    CHECK(std::strcmp(select.column_name(0), "name") == 0);

    CHECK(select.step().row());
    CHECK(select.column_text(0) == "Alice");
    CHECK(select.column_text16(0) == u"Alice");
    CHECK(select.column_int(1) == 31);
    CHECK(select.column_double(2) == 1.5);
    CHECK(select.column_type(3) == value_type::blob);
    CHECK(select.column_bytes(3) == 3);
    CHECK(std::memcmp(select.column_blob(3), bytes, 3) == 0);

    CHECK(select.step().row());
    // An empty string is text, so an empty view, never nullopt; NULL is nullopt
    CHECK(select.column_type(0) == value_type::text);
    CHECK(select.column_text(0).has_value());
    CHECK(select.column_text(0) && select.column_text(0)->empty());
    CHECK(select.column_int64(1) == 5000000000LL);
    CHECK(select.column_type(2) == value_type::null);
    CHECK(!select.column_text(2));
    CHECK(!select.column_text16(2));

    CHECK(select.step().done());
}

void test_text_is_terminated()
{
    connection db;
    CHECK(db.open(":memory:").ok());

    statement stmt;
    CHECK(db.prepare("SELECT ?, ?, x'', 42", stmt).ok());
    // Bound without a terminator: only the first two characters of "abc"
    CHECK(stmt.bind_text(1, string_view("abc", 2)).ok());
    // A '\0' inside the text
    CHECK(stmt.bind_text(2, string_view("a\0b", 3)).ok());
    CHECK(stmt.step().row());

    sqlitexx::optional<sqlitexx::cstring_view> text = stmt.column_text(0);
    CHECK(text && text->size() == 2);
    CHECK(text && std::strcmp(text->c_str(), "ab") == 0);
    CHECK(text && (*text)[2] == '\0');

    // size() counts the inner '\0'; a C function stops at it
    text = stmt.column_text(1);
    CHECK(text && text->size() == 3);
    CHECK(text && std::strlen(text->c_str()) == 1);
    CHECK(text && (*text)[3] == '\0');

    // A zero-length blob read as text is an empty string, not "no text"
    CHECK(stmt.column_blob(2) == nullptr);
    text = stmt.column_text(2);
    CHECK(text && text->empty() && *text->c_str() == '\0');

    // Numbers convert to text
    CHECK(stmt.column_text(3) == "42");
    CHECK(stmt.column_text16(3) == u"42");
}

void test_prepare_errors_and_tail()
{
    connection db;
    CHECK(db.open(":memory:").ok());

    statement stmt;
    const result rc = db.prepare("SELEKT 1", stmt);
    CHECK(rc == primary_code::error);
    CHECK(!stmt);
    CHECK(std::strstr(db.errmsg(), "syntax error") != nullptr);

    string_view tail;
    CHECK(db.prepare("SELECT 1; SELECT 2", stmt, prepare_flags::none, &tail).ok());
    CHECK(tail == " SELECT 2");

    // Only whitespace: ok, but no statement
    CHECK(db.prepare("   ", stmt).ok());
    CHECK(!stmt);

    u16string_view tail16;
    CHECK(db.prepare(u16string_view(u"SELECT 'x'; --"), stmt, prepare_flags::none, &tail16).ok());
    CHECK(stmt.step().row());
    CHECK(stmt.column_text(0) == "x");
    CHECK(tail16 == u" --");
}

void test_constraint_is_extended()
{
    connection db;
    CHECK(db.open(":memory:").ok());
    CHECK(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT NOT NULL UNIQUE)").ok());
    CHECK(db.exec("INSERT INTO t(v) VALUES('a')").ok());

    statement insert;
    CHECK(db.prepare("INSERT INTO t(v) VALUES('a')", insert).ok());
    const result rc = insert.step();
    CHECK(rc == primary_code::constraint);
    CHECK(rc.code() != static_cast<int>(primary_code::constraint)); // SQLITE_CONSTRAINT_UNIQUE, not the bare code
    CHECK(db.extended_errcode() == rc);
}

void test_moves_and_statement_outliving_connection()
{
    statement stmt;
    {
        connection db;
        CHECK(db.open(":memory:").ok());
        connection moved(std::move(db));
        CHECK(!db);
        CHECK(static_cast<bool>(moved));
        CHECK(moved.prepare("SELECT 42", stmt).ok());
    }
    // close_v2 left the connection as a zombie, so the statement still works
    CHECK(stmt.step().row());
    CHECK(stmt.column_int(0) == 42);

    statement other = std::move(stmt);
    CHECK(!stmt);
    CHECK(static_cast<bool>(other));
    CHECK(other.finalize().ok());
    CHECK(!other);
}
} // anonymous

int main()
{
    test_open_and_exec();
    test_failed_open_keeps_message();
    test_bind_step_and_columns();
    test_text_is_terminated();
    test_prepare_errors_and_tail();
    test_constraint_is_extended();
    test_moves_and_statement_outliving_connection();

    return test::summary();
}
