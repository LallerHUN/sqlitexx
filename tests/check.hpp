#ifndef SQLITEXX_TESTS_CHECK_HPP
#define SQLITEXX_TESTS_CHECK_HPP

// Minimal test checks that stay on in release builds; a real test framework comes later.

#include <cstdio>

namespace test
{
inline int& failures()
{
    static int count = 0;
    return count;
}

inline void check(
    bool condition,
    const char* expression,
    int line
)
{
    if (!condition)
    {
        std::printf("FAILED line %d: %s\n", line, expression);
        ++failures();
    }
}

// The exit code for main()
inline int summary()
{
    if (failures() != 0)
    {
        std::printf("%d check(s) failed\n", failures());
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
} // test

#define CHECK(expression) ::test::check((expression), #expression, __LINE__)

#endif // SQLITEXX_TESTS_CHECK_HPP
