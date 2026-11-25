#ifndef SQLITEXX_TESTS_CHECK_HPP
#define SQLITEXX_TESTS_CHECK_HPP

// Minimal test checks that stay on in release builds; a real test framework comes later.

#include <cstdio>

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <cstdlib>
#endif

namespace test
{
#if defined(_MSC_VER)
// abort() and failed CRT checks report on stderr and end the test: in a Debug build they would wait on a dialog box,
// and CI with them
struct no_dialogs
{
    no_dialogs()
    {
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    }
};

static const no_dialogs report_without_dialogs;
#endif

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
