// Tests for the types from newer standards: std::byte blobs (C++17), std::span blobs and char8_t text (C++20). Each
// part runs only when the standard has the type, so this program builds in every standard.

#include "check.hpp"
#include "sqlnx/sqlite.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

using sqlnx::sqlite::database;
using sqlnx::sqlite::expected;
using sqlnx::sqlite::statement;

namespace
{
int parts_run = 0;

#if defined(__cpp_lib_byte) || (defined(__cpp_char8_t) && defined(__cpp_lib_char8_t))
// A statement that hands back what was bound to it, or an empty one when preparing fails (the checks report it)
statement echo(database& db)
{
    expected<statement> select = db.prepare("SELECT ?, typeof(?1)");
    CHECK(select.has_value());
    if (!select)
    {
        return statement();
    }
    return std::move(*select);
}

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
#endif

#if defined(__cpp_lib_byte)
void test_byte_vector()
{
    ++parts_run;
    database db = open_memory();
    statement select = echo(db);

    const std::vector<std::byte> bytes = {std::byte{0x00}, std::byte{0xFF}, std::byte{0x7F}};
    CHECK(select.bind(bytes).has_value());
    expected<std::tuple<std::vector<std::byte>, std::string>> row = select.first<std::vector<std::byte>, std::string>();
    CHECK(row.has_value());
    CHECK(row && std::get<0>(*row) == bytes);
    CHECK(row && std::get<1>(*row) == "blob");

    // An empty vector is an empty blob, not NULL
    CHECK(select.reset().has_value());
    CHECK(select.bind(std::vector<std::byte>()).has_value());
    row = select.first<std::vector<std::byte>, std::string>();
    CHECK(row && std::get<0>(*row).empty());
    CHECK(row && std::get<1>(*row) == "blob");
}
#endif

#if defined(__cpp_lib_span) && defined(__cpp_lib_byte)
void test_spans()
{
    ++parts_run;
    database db = open_memory();
    statement select = echo(db);

    const unsigned char raw[] = {1, 2, 3, 4};

    // Fixed extent, unsigned char: binds without a copy into a blob first
    CHECK(select.bind(std::span<const unsigned char, 4>(raw)).has_value());
    expected<std::span<const std::byte>> view = select.first<std::span<const std::byte>>();
    CHECK(view && view->size() == 4);
    CHECK(view && std::memcmp(view->data(), raw, 4) == 0);

    // Writable bytes bind too; reading as unsigned char
    std::byte writable[] = {std::byte{9}, std::byte{8}};
    CHECK(select.reset().has_value());
    CHECK(select.bind(std::span<std::byte>(writable)).has_value());
    expected<std::span<const unsigned char>> chars = select.first<std::span<const unsigned char>>();
    CHECK(chars && chars->size() == 2 && (*chars)[0] == 9 && (*chars)[1] == 8);

    // Empty: an empty blob, read back as an empty span
    CHECK(select.reset().has_value());
    CHECK(select.bind(std::span<const std::byte>()).has_value());
    expected<std::tuple<std::span<const std::byte>, std::string>> empty =
        select.first<std::span<const std::byte>, std::string>();
    CHECK(empty && std::get<0>(*empty).empty());
    CHECK(empty && std::get<1>(*empty) == "blob");

    // NULL needs an optional
    CHECK(select.reset().has_value());
    CHECK(select.bind(nullptr).has_value());
    expected<std::span<const std::byte>> strict = select.first<std::span<const std::byte>>();
    CHECK(!strict && strict.error().primary() == sqlnx::sqlite::native::primary_code::mismatch);
}
#endif

#if defined(__cpp_char8_t) && defined(__cpp_lib_char8_t)
void test_char8_text()
{
    ++parts_run;
    database db = open_memory();
    statement select = echo(db);

    // A u8 literal binds as UTF-8 text, the same bytes as the char literal
    CHECK(select.bind(u8"Zoë").has_value());
    expected<std::tuple<std::string, std::string>> plain = select.first<std::string, std::string>();
    CHECK(plain && std::get<0>(*plain) == "Zo\xC3\xAB");
    CHECK(plain && std::get<1>(*plain) == "text");

    CHECK(select.reset().has_value());
    CHECK(select.bind(std::u8string(u8"été")).has_value());
    expected<std::u8string> owned = select.first<std::u8string>();
    CHECK(owned && *owned == u8"été");

    CHECK(select.reset().has_value());
    CHECK(select.bind(std::u8string_view(u8"view")).has_value());
    expected<std::u8string_view> view = select.first<std::u8string_view>();
    CHECK(view && *view == u8"view");

    CHECK(select.reset().has_value());
    CHECK(select.bind(sqlnx::sqlite::u8cstring_view(u8"terminated")).has_value());
    expected<sqlnx::sqlite::u8cstring_view> terminated = select.first<sqlnx::sqlite::u8cstring_view>();
    CHECK(terminated && terminated->size() == 10);
    CHECK(terminated && terminated->c_str()[10] == u8'\0');
    CHECK(terminated && *terminated == std::u8string_view(u8"terminated"));

    // The empty string is text; a null pointer binds NULL
    CHECK(select.reset().has_value());
    CHECK(select.bind(u8"").has_value());
    expected<std::tuple<std::u8string, std::string>> empty = select.first<std::u8string, std::string>();
    CHECK(empty && std::get<0>(*empty).empty());
    CHECK(empty && std::get<1>(*empty) == "text");

    CHECK(select.reset().has_value());
    const char8_t* none = nullptr;
    CHECK(select.bind(none).has_value());
    expected<sqlnx::sqlite::optional<std::u8string>> null = select.first<sqlnx::sqlite::optional<std::u8string>>();
    CHECK(null && !null->has_value());
}
#endif
} // anonymous

int main()
{
#if defined(__cpp_lib_byte)
    test_byte_vector();
#endif
#if defined(__cpp_lib_span) && defined(__cpp_lib_byte)
    test_spans();
#endif
#if defined(__cpp_char8_t) && defined(__cpp_lib_char8_t)
    test_char8_text();
#endif
    std::printf("%d of 3 parts ran in this standard\n", parts_run);
    return test::summary();
}
