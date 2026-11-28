// Tests for the native layer.

#include "check.hpp"
#include "sqlnx/sqlite.hpp"

#include <cstring>
#include <utility>

using namespace sqlnx::sqlite::native;
using sqlnx::sqlite::string_view;
using sqlnx::sqlite::u16string_view;

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

    sqlnx::sqlite::optional<sqlnx::sqlite::cstring_view> text = stmt.column_text(0);
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
    CHECK(rc == extended_code::constraint_unique);
    CHECK(!(rc != extended_code::constraint_unique));
    CHECK(rc != extended_code::constraint_primarykey);
    CHECK(db.extended_errcode() == rc);
}

void test_backup()
{
    connection source;
    connection destination;
    CHECK(source.open(":memory:").ok());
    CHECK(destination.open(":memory:").ok());
    CHECK(source.exec("CREATE TABLE t(v); INSERT INTO t VALUES(1), (2)").ok());

    backup copy;
    CHECK(!copy);
    CHECK(copy.init(destination, "main", source, "main").ok());
    CHECK(static_cast<bool>(copy));
    CHECK(copy.step(-1).done());
    CHECK(copy.remaining() == 0 && copy.pagecount() > 0);
    CHECK(copy.finish().ok());
    CHECK(!copy);

    statement count;
    CHECK(destination.prepare("SELECT count(*) FROM t", count).ok());
    CHECK(count.step().row() && count.column_int(0) == 2);

    // A failed init holds nothing; the destination has the message
    const result failed = copy.init(destination, "main", source, "nowhere");
    CHECK(failed == primary_code::error);
    CHECK(!copy);
    CHECK(std::strstr(destination.errmsg(), "nowhere") != nullptr);
}

void test_blob_handle()
{
    connection db;
    CHECK(db.open(":memory:").ok());
    CHECK(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, data BLOB); INSERT INTO t VALUES(1, zeroblob(4))").ok());

    blob_handle blob;
    CHECK(!blob);
    CHECK(blob.open(db, "main", "t", "data", 1, true).ok());
    CHECK(static_cast<bool>(blob));
    CHECK(blob.bytes() == 4);
    const unsigned char written[2] = {7, 9};
    CHECK(blob.write(written, 2, 1).ok());
    unsigned char read[4] = {1, 1, 1, 1};
    CHECK(blob.read(read, 4, 0).ok());
    CHECK(read[0] == 0 && read[1] == 7 && read[2] == 9 && read[3] == 0);

    // Past the end fails; the blob keeps its size
    CHECK(blob.write(written, 2, 3) == primary_code::error);
    CHECK(blob.close().ok());
    CHECK(!blob);

    // A failed open holds nothing; the connection has the message
    CHECK(blob.open(db, "main", "t", "nowhere", 1, false) == primary_code::error);
    CHECK(!blob);
    CHECK(std::strstr(db.errmsg(), "nowhere") != nullptr);
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
void test_empty_statement()
{
    // An empty statement answers without reaching SQLite, which wouldn't check the handle
    sqlnx::sqlite::native::statement empty;
    CHECK(empty.status(sqlnx::sqlite::native::statement_status::vm_step) == 0);
    CHECK(empty.status(sqlnx::sqlite::native::statement_status::memused) == 0);
    CHECK(empty.status(sqlnx::sqlite::native::statement_status::filter_hit, true) == 0);
    CHECK(empty.column_name(0) == nullptr);
    CHECK(empty.column_decltype(0) == nullptr);
    CHECK(empty.column_count() == 0);
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
    test_backup();
    test_blob_handle();
    test_empty_statement();

    return test::summary();
}
