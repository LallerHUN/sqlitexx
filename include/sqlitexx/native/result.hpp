#ifndef SQLITEXX_NATIVE_RESULT_HPP
#define SQLITEXX_NATIVE_RESULT_HPP

namespace sqlitexx
{
namespace native
{
    // The primary result codes (the low 8 bits of an extended code). Values match SQLITE_* in sqlite3.h.
    enum class primary_code : int
    {
        ok = 0,
        error = 1,
        internal = 2,
        perm = 3,
        abort = 4,
        busy = 5,
        locked = 6,
        nomem = 7,
        readonly = 8,
        interrupt = 9,
        ioerr = 10,
        corrupt = 11,
        notfound = 12,
        full = 13,
        cantopen = 14,
        protocol = 15,
        empty = 16,
        schema = 17,
        toobig = 18,
        constraint = 19,
        mismatch = 20,
        misuse = 21,
        nolfs = 22,
        auth = 23,
        format = 24,
        range = 25,
        notadb = 26,
        notice = 27,
        warning = 28,
        row = 100,
        done = 101,
    };

    // A result code returned by the native layer. Connections opened through sqlitexx always report extended result
    // codes, so code() is the extended code and primary() its primary part.
    class result final
    {
    public:
        constexpr result() noexcept = default;

        constexpr explicit result(int code) noexcept
            : value(code)
        {
        }

        constexpr int code() const noexcept
        {
            return value;
        }

        constexpr primary_code primary() const noexcept
        {
            return static_cast<primary_code>(value & 0xff);
        }

        // SQLITE_OK
        constexpr bool ok() const noexcept
        {
            return value == static_cast<int>(primary_code::ok);
        }

        // SQLITE_ROW: step() produced a row
        constexpr bool row() const noexcept
        {
            return value == static_cast<int>(primary_code::row);
        }

        // SQLITE_DONE: step() finished
        constexpr bool done() const noexcept
        {
            return value == static_cast<int>(primary_code::done);
        }

        // Anything other than ok, row or done
        constexpr bool failed() const noexcept
        {
            return !ok() && !row() && !done();
        }

        // English description of the code (sqlite3_errstr). It does not depend on any connection; for the message of
        // the failed call itself use connection::errmsg().
        const char* errstr() const noexcept;

    private:
        int value = 0;
    };

    constexpr bool operator==(
        result lhs,
        result rhs
    ) noexcept
    {
        return lhs.code() == rhs.code();
    }

    constexpr bool operator!=(
        result lhs,
        result rhs
    ) noexcept
    {
        return lhs.code() != rhs.code();
    }

    constexpr bool operator==(
        result lhs,
        primary_code rhs
    ) noexcept
    {
        return lhs.primary() == rhs;
    }

    constexpr bool operator!=(
        result lhs,
        primary_code rhs
    ) noexcept
    {
        return lhs.primary() != rhs;
    }

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_RESULT_HPP
