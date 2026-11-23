#include "impl_sqlitexx.hpp"

namespace sqlitexx
{
namespace native
{
    // The public headers do not include sqlite3.h, so they repeat its constants. Keep them honest.
    // clang-format off: these are assertions, not the function declarations clang-format takes them for
    static_assert(static_cast<int>(primary_code::ok) == SQLITE_OK, "");
    static_assert(static_cast<int>(primary_code::error) == SQLITE_ERROR, "");
    static_assert(static_cast<int>(primary_code::busy) == SQLITE_BUSY, "");
    static_assert(static_cast<int>(primary_code::locked) == SQLITE_LOCKED, "");
    static_assert(static_cast<int>(primary_code::nomem) == SQLITE_NOMEM, "");
    static_assert(static_cast<int>(primary_code::ioerr) == SQLITE_IOERR, "");
    static_assert(static_cast<int>(primary_code::cantopen) == SQLITE_CANTOPEN, "");
    static_assert(static_cast<int>(primary_code::toobig) == SQLITE_TOOBIG, "");
    static_assert(static_cast<int>(primary_code::constraint) == SQLITE_CONSTRAINT, "");
    static_assert(static_cast<int>(primary_code::misuse) == SQLITE_MISUSE, "");
    static_assert(static_cast<int>(primary_code::range) == SQLITE_RANGE, "");
    static_assert(static_cast<int>(primary_code::notadb) == SQLITE_NOTADB, "");
    static_assert(static_cast<int>(primary_code::warning) == SQLITE_WARNING, "");
    static_assert(static_cast<int>(primary_code::row) == SQLITE_ROW, "");
    static_assert(static_cast<int>(primary_code::done) == SQLITE_DONE, "");

    static_assert(static_cast<int>(open_flags::readonly) == SQLITE_OPEN_READONLY, "");
    static_assert(static_cast<int>(open_flags::readwrite) == SQLITE_OPEN_READWRITE, "");
    static_assert(static_cast<int>(open_flags::create) == SQLITE_OPEN_CREATE, "");
    static_assert(static_cast<int>(open_flags::uri) == SQLITE_OPEN_URI, "");
    static_assert(static_cast<int>(open_flags::memory) == SQLITE_OPEN_MEMORY, "");
    static_assert(static_cast<int>(open_flags::nomutex) == SQLITE_OPEN_NOMUTEX, "");
    static_assert(static_cast<int>(open_flags::fullmutex) == SQLITE_OPEN_FULLMUTEX, "");
    static_assert(static_cast<int>(open_flags::sharedcache) == SQLITE_OPEN_SHAREDCACHE, "");
    static_assert(static_cast<int>(open_flags::privatecache) == SQLITE_OPEN_PRIVATECACHE, "");
    static_assert(static_cast<int>(open_flags::nofollow) == SQLITE_OPEN_NOFOLLOW, "");

    static_assert(static_cast<unsigned int>(prepare_flags::persistent) == SQLITE_PREPARE_PERSISTENT, "");
    static_assert(static_cast<unsigned int>(prepare_flags::normalize) == SQLITE_PREPARE_NORMALIZE, "");
    static_assert(static_cast<unsigned int>(prepare_flags::no_vtab) == SQLITE_PREPARE_NO_VTAB, "");

    static_assert(static_cast<int>(value_type::integer) == SQLITE_INTEGER, "");
    static_assert(static_cast<int>(value_type::floating) == SQLITE_FLOAT, "");
    static_assert(static_cast<int>(value_type::text) == SQLITE_TEXT, "");
    static_assert(static_cast<int>(value_type::blob) == SQLITE_BLOB, "");
    static_assert(static_cast<int>(value_type::null) == SQLITE_NULL, "");
    // clang-format on

    const char* result::errstr() const noexcept
    {
        return sqlite3_errstr(value);
    }

} // native
} // sqlitexx
