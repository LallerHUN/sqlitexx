// Tests for cstring_view on its own, without SQLite.

#include "check.hpp"
#include "sqlnx/sqlite/cstring_view.hpp"

#include <cstring>
#include <string>

using sqlnx::sqlite::cstring_view;
using sqlnx::sqlite::string_view;
using sqlnx::sqlite::u16cstring_view;

namespace
{
void test_construction()
{
    const cstring_view empty;
    CHECK(empty.empty());
    CHECK(empty.c_str() != nullptr);
    CHECK(*empty.c_str() == '\0');

    const char* none = nullptr;
    const cstring_view from_null(none);
    CHECK(from_null.empty());
    CHECK(from_null.c_str() != nullptr);

    const cstring_view literal("hello");
    CHECK(literal.size() == 5);
    CHECK(literal.length() == 5);
    CHECK(literal[5] == '\0');

    const std::string text("abc");
    const cstring_view from_string(text);
    CHECK(from_string.data() == text.c_str());
    CHECK(from_string.size() == 3);

    const char buffer[] = {'x', 'y', '\0'};
    const cstring_view assumed = cstring_view::assume_terminated(buffer, 2);
    CHECK(assumed.data() == buffer);
    CHECK(assumed.size() == 2);

    const u16cstring_view wide(u"wide");
    CHECK(wide.size() == 4);
    CHECK(wide == u"wide");
}

void test_remove_prefix_keeps_terminator()
{
    cstring_view text("prefix:value");
    text.remove_prefix(7);
    CHECK(text == "value");
    CHECK(std::strcmp(text.c_str(), "value") == 0);
    text.remove_prefix(text.size());
    CHECK(text.empty());
    CHECK(*text.c_str() == '\0');
}

void test_iteration_and_view()
{
    const cstring_view text("abc");
    std::string copy(text.begin(), text.end());
    CHECK(copy == "abc");

    const string_view view = text;
    CHECK(view.size() == 3);
    CHECK(text.view().substr(1, 1) == "b");
}

void test_comparisons()
{
    const cstring_view a("same");
    const char same[] = "same";
    const cstring_view b(same);
    const cstring_view c("other");
    const std::string s("same");
    const string_view v("same");

    CHECK(a == b);
    CHECK(!(a != b));
    CHECK(a != c);
    CHECK(a == "same");
    CHECK("same" == a);
    CHECK(a != "other");
    CHECK("other" != a);
    CHECK(a == s);
    CHECK(s == a);
    CHECK(a == v);
    CHECK(v == a);
    CHECK(c != v);
    CHECK(v != c);
}
} // anonymous

int main()
{
    test_construction();
    test_remove_prefix_keeps_terminator();
    test_iteration_and_view();
    test_comparisons();
    return test::summary();
}
