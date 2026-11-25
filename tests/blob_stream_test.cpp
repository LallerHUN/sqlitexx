// Tests for database::open_blob and blob_stream: reading and writing blobs in pieces.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#if SQLITEXX_CPLUSPLUS >= 202002L && defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

using sqlitexx::blob;
using sqlitexx::blob_access;
using sqlitexx::blob_stream;
using sqlitexx::database;
using sqlitexx::expected;
using sqlitexx::zeroblob;
using sqlitexx::native::primary_code;

namespace
{
// A table with one row per test value: id 1 holds `first`, id 2 holds `second`
database open_files(
    const blob& first,
    const blob& second
)
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return database();
    }
    CHECK(db->execute("CREATE TABLE file(id INTEGER PRIMARY KEY, data BLOB)").has_value());
    expected<sqlitexx::statement> insert = db->prepare("INSERT INTO file(data) VALUES(?)");
    CHECK(insert && insert->execute(first).has_value());
    CHECK(insert && insert->execute(second).has_value());
    return std::move(*db);
}

blob counting(std::size_t size)
{
    blob data(size);
    for (std::size_t i = 0; i < size; ++i)
    {
        data[i] = static_cast<unsigned char>(i % 251);
    }
    return data;
}

blob stored(
    database& db,
    std::int64_t id
)
{
    return db.prepare("SELECT data FROM file WHERE id = ?")
        .and_then(sqlitexx::bind_values(id))
        .and_then(sqlitexx::first<blob>())
        .value_or(blob());
}

void test_read_in_pieces()
{
    const blob data = counting(10000);
    database db = open_files(data, blob());

    expected<blob_stream> in = db.open_blob("file", "data", 1);
    CHECK(in.has_value());
    if (!in)
    {
        return;
    }
    CHECK(in->size() == 10000);
    CHECK(in->position() == 0);

    // Fixed-size pieces: the last one is short, then reads give 0
    blob read;
    unsigned char buffer[4096];
    int pieces = 0;
    for (;;)
    {
        expected<std::size_t> got = in->read(buffer, sizeof buffer);
        CHECK(got.has_value());
        if (!got || *got == 0)
        {
            break;
        }
        read.insert(read.end(), buffer, buffer + *got);
        ++pieces;
    }
    CHECK(pieces == 3);
    CHECK(read == data);
    CHECK(in->position() == 10000);

    // Seeking anywhere up to the end, and no further
    CHECK(in->seek(9998).has_value());
    expected<std::size_t> tail = in->read(buffer, sizeof buffer);
    CHECK(tail && *tail == 2 && buffer[0] == data[9998] && buffer[1] == data[9999]);
    CHECK(in->seek(10000).has_value());
    expected<void> beyond = in->seek(10001);
    CHECK(!beyond && beyond.error().primary() == primary_code::range);
    CHECK(in->position() == 10000);
}

void test_write_into_reserved_space()
{
    database db = open_files(blob(), blob());

    // zeroblob reserves the space without building it in memory
    CHECK(db.prepare("INSERT INTO file(id, data) VALUES(3, ?)")
              .and_then(sqlitexx::execute_with(zeroblob{3000}))
              .has_value());
    expected<std::string> type = db.prepare("SELECT typeof(data) || length(data) FROM file WHERE id = 3")
                                     .and_then(sqlitexx::first<std::string>());
    CHECK(type && *type == "blob3000");

    const blob data = counting(3000);
    {
        expected<blob_stream> out = db.open_blob("file", "data", 3, blob_access::read_write);
        CHECK(out.has_value());
        if (!out)
        {
            return;
        }
        for (std::size_t at = 0; at < data.size(); at += 1000)
        {
            CHECK(out->write(data.data() + at, 1000).has_value());
        }
        CHECK(out->position() == 3000);

        // The size is fixed: writing past the end writes nothing
        const unsigned char extra[2] = {1, 2};
        CHECK(out->seek(2999).has_value());
        expected<void> past = out->write(extra, 2);
        CHECK(!past && past.error().primary() == primary_code::range);
        CHECK(out->position() == 2999);

        CHECK(out->close().has_value());
        CHECK(out->size() == 0);
    }
    CHECK(stored(db, 3) == data);
}

void test_reopen_and_access()
{
    const blob first = counting(10);
    blob second = counting(20);
    second[0] = 0xEE;
    database db = open_files(first, second);

    expected<blob_stream> stream = db.open_blob("file", "data", 1);
    CHECK(stream.has_value());
    if (!stream)
    {
        return;
    }

    // Another row of the same column, from position 0
    unsigned char byte = 0;
    CHECK(stream->seek(5).has_value());
    CHECK(stream->reopen(2).has_value());
    CHECK(stream->size() == 20 && stream->position() == 0);
    expected<std::size_t> got = stream->read(&byte, 1);
    CHECK(got && *got == 1 && byte == 0xEE);

    // Read-only streams don't write
    expected<void> refused = stream->write(&byte, 1);
    CHECK(!refused && refused.error().primary() == primary_code::readonly);

    // A row that isn't there
    expected<void> missing = stream->reopen(99);
    CHECK(!missing && missing.error().primary() == primary_code::error);
}

void test_expired_stream()
{
    database db = open_files(counting(100), counting(100));
    expected<blob_stream> stream = db.open_blob("file", "data", 1);
    CHECK(stream.has_value());
    if (!stream)
    {
        return;
    }

    // Changing the row through a statement expires the stream
    CHECK(db.execute("UPDATE file SET data = x'00' WHERE id = 1").has_value());
    unsigned char byte = 0;
    expected<std::size_t> got = stream->read(&byte, 1);
    CHECK(!got && got.error().primary() == primary_code::abort);
}

void test_open_errors()
{
    database db = open_files(counting(1), blob());
    CHECK(db.execute("INSERT INTO file(id, data) VALUES(5, NULL)").has_value());

    expected<blob_stream> no_table = db.open_blob("nothing", "data", 1);
    CHECK(!no_table && no_table.error().primary() == primary_code::error);
    CHECK(!no_table && no_table.error().message().find("nothing") != std::string::npos);

    expected<blob_stream> no_column = db.open_blob("file", "nothing", 1);
    CHECK(!no_column && no_column.error().primary() == primary_code::error);

    expected<blob_stream> no_row = db.open_blob("file", "data", 42);
    CHECK(!no_row && no_row.error().primary() == primary_code::error);

    expected<blob_stream> null_value = db.open_blob("file", "data", 5);
    CHECK(!null_value && null_value.error().primary() == primary_code::error);

    database closed;
    expected<blob_stream> nowhere = closed.open_blob("file", "data", 1);
    CHECK(!nowhere && nowhere.error().primary() == primary_code::misuse);

    // A stream that isn't open
    blob_stream none;
    unsigned char byte = 0;
    expected<std::size_t> got = none.read(&byte, 1);
    CHECK(!got && got.error().primary() == primary_code::misuse);
    CHECK(none.size() == 0 && none.position() == 0);
    CHECK(none.close().has_value());
}
// Which containers count as bytes
static_assert(
    sqlitexx::detail::is_readable_bytes<std::vector<unsigned char>>::value,
    ""
);
static_assert(
    sqlitexx::detail::is_readable_bytes<std::string>::value,
    ""
);
static_assert(
    sqlitexx::detail::is_readable_bytes<std::array<
        char,
        4>>::value,
    ""
);
static_assert(
    sqlitexx::detail::is_readable_bytes<sqlitexx::string_view>::value,
    ""
);
static_assert(
    !sqlitexx::detail::is_readable_bytes<std::vector<int>>::value,
    "wider elements depend on the machine"
);
static_assert(
    sqlitexx::detail::is_growable_bytes<std::vector<unsigned char>>::value,
    ""
);
static_assert(
    sqlitexx::detail::is_growable_bytes<std::string>::value,
    ""
);
static_assert(
    !sqlitexx::detail::is_growable_bytes<const std::vector<unsigned char>>::value,
    "const can't grow"
);
static_assert(
    !sqlitexx::detail::is_growable_bytes<std::array<
        unsigned char,
        4>>::value,
    ""
);
static_assert(
    sqlitexx::detail::is_fixed_bytes<std::array<
        unsigned char,
        4>>::value,
    ""
);
static_assert(
    !sqlitexx::detail::is_fixed_bytes<const std::array<
        unsigned char,
        4>>::value,
    "const can't be filled"
);
static_assert(
    !sqlitexx::detail::is_fixed_bytes<std::vector<unsigned char>>::value,
    "a vector grows instead"
);
static_assert(
    !sqlitexx::detail::is_byte_array_element<char>::value,
    "a string literal would write its zero"
);
static_assert(
    sqlitexx::detail::is_byte_array_element<unsigned char>::value,
    ""
);

void test_containers()
{
    database db = open_files(blob(), blob());
    CHECK(
        db.prepare("INSERT INTO file(id, data) VALUES(3, ?)").and_then(sqlitexx::execute_with(zeroblob{16})).has_value()
    );

    {
        expected<blob_stream> out = db.open_blob("file", "data", 3, blob_access::read_write);
        CHECK(out.has_value());
        if (!out)
        {
            return;
        }
        // Whole containers, a prefix of one, and a C array
        const std::vector<unsigned char> head = {1, 2, 3, 4};
        const std::array<char, 2> pair = {{'a', 'b'}};
        const std::string text = "xyz-not-this";
        const unsigned char raw[3] = {7, 8, 9};
        CHECK(out->write(head).has_value());
        CHECK(out->write(pair).has_value());
        CHECK(out->write(text, 3).has_value());
        CHECK(out->write(raw).has_value());
        CHECK(out->write(raw, 2).has_value());
        CHECK(out->write(sqlitexx::string_view("!!")).has_value());
        CHECK(out->position() == 16);

        // A count larger than the container is an error, before anything is written
        CHECK(out->seek(0).has_value());
        expected<void> too_many = out->write(head, 5);
        CHECK(!too_many && too_many.error().primary() == primary_code::range);
        CHECK(out->position() == 0);
    }

    expected<blob_stream> in = db.open_blob("file", "data", 3);
    CHECK(in.has_value());
    if (!in)
    {
        return;
    }

    // A growable container is sized to what was read
    std::vector<unsigned char> first;
    expected<std::size_t> got = in->read(first, 4);
    CHECK(got && *got == 4 && first == std::vector<unsigned char>({1, 2, 3, 4}));
    std::string letters;
    got = in->read(letters, 5);
    CHECK(got && *got == 5 && letters == "abxyz");

    // A fixed buffer is filled up to its size, or to a count that fits it
    std::array<unsigned char, 3> three = {{0, 0, 0}};
    got = in->read(three);
    CHECK(got && *got == 3 && three[0] == 7 && three[1] == 8 && three[2] == 9);
    unsigned char two[2] = {0, 0};
    got = in->read(two, 1);
    CHECK(got && *got == 1 && two[0] == 7);
    expected<std::size_t> overflow = in->read(two, 3);
    CHECK(!overflow && overflow.error().primary() == primary_code::range);

    // The rest, then nothing left: an empty container
    std::vector<unsigned char> rest;
    got = in->read(rest);
    CHECK(got && *got == 3 && rest.size() == 3 && rest[0] == 8 && rest[1] == '!' && rest[2] == '!');
    got = in->read(rest);
    CHECK(got && *got == 0 && rest.empty());

    // A fixed buffer near the end gets only what is left
    CHECK(in->seek(14).has_value());
    std::array<unsigned char, 8> wide = {{}};
    got = in->read(wide);
    CHECK(got && *got == 2 && wide[0] == '!' && wide[1] == '!');
}

#if defined(__cpp_lib_byte)
void test_byte_containers()
{
    database db = open_files(blob(4), blob());
    expected<blob_stream> stream = db.open_blob("file", "data", 1, blob_access::read_write);
    CHECK(stream.has_value());
    if (!stream)
    {
        return;
    }
    const std::vector<std::byte> bytes = {std::byte{0xCA}, std::byte{0xFE}};
    CHECK(stream->write(bytes).has_value());
    CHECK(stream->seek(0).has_value());
    std::vector<std::byte> back;
    expected<std::size_t> got = stream->read(back, 2);
    CHECK(got && *got == 2 && back == bytes);

#if defined(__cpp_lib_span)
    // Spans: writable ones are filled like fixed buffers, any of them can be written
    std::array<std::byte, 2> storage = {};
    std::span<std::byte> view(storage);
    CHECK(stream->seek(0).has_value());
    got = stream->read(view);
    CHECK(got && *got == 2 && storage[0] == std::byte{0xCA});
    CHECK(stream->seek(2).has_value());
    CHECK(stream->write(std::span<const std::byte>(storage)).has_value());
#endif
}
#endif
} // anonymous

int main()
{
    test_read_in_pieces();
    test_write_into_reserved_space();
    test_reopen_and_access();
    test_expired_stream();
    test_open_errors();
    test_containers();
#if defined(__cpp_lib_byte)
    test_byte_containers();
#endif
    return test::summary();
}
