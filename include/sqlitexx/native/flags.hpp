#ifndef SQLITEXX_NATIVE_FLAGS_HPP
#define SQLITEXX_NATIVE_FLAGS_HPP

namespace sqlitexx
{
namespace native
{
    /// Flags for connection::open. Values match SQLITE_OPEN_* in sqlite3.h.
    enum class open_flags : int
    {
        none = 0,
        readonly = 0x00000001,
        readwrite = 0x00000002,
        create = 0x00000004,
        uri = 0x00000040,
        memory = 0x00000080,
        nomutex = 0x00008000,
        fullmutex = 0x00010000,
        sharedcache = 0x00020000,
        privatecache = 0x00040000,
        nofollow = 0x01000000,

        // What sqlite3_open uses
        default_rw = readwrite | create,
    };

    /// Flags for connection::prepare. Values match SQLITE_PREPARE_* in sqlite3.h.
    enum class prepare_flags : unsigned int
    {
        none = 0,
        persistent = 0x01,
        normalize = 0x02,
        no_vtab = 0x04,
    };

    /// How an SQL function behaves (connection::create_function). Values match SQLITE_DETERMINISTIC,
    /// SQLITE_DIRECTONLY and SQLITE_INNOCUOUS in sqlite3.h; the text encoding is always UTF-8.
    enum class function_flags : int
    {
        none = 0,
        // The same arguments always give the same result, so SQLite may evaluate it once and use it in indexes
        deterministic = 0x00000800,
        // Only callable from top-level SQL, not from triggers, views, CHECK constraints or generated columns
        directonly = 0x00080000,
        // Harmless even when called from a schema someone else wrote
        innocuous = 0x00200000,
    };

    /// How a bound text or blob is held by SQLite.
    enum class bind_mode
    {
        // SQLite copies the data before the bind call returns (SQLITE_TRANSIENT).
        copy,
        // SQLite keeps a pointer (SQLITE_STATIC): the caller keeps the data alive and unchanged until it is rebound,
        // the bindings are cleared or the statement is finalized.
        no_copy,
    };

    /// The fundamental datatype of a column value. Values match SQLITE_INTEGER ... SQLITE_NULL in sqlite3.h.
    enum class value_type : int
    {
        integer = 1,
        floating = 2,
        text = 3,
        blob = 4,
        null = 5,
    };

    constexpr open_flags operator|(
        open_flags lhs,
        open_flags rhs
    ) noexcept
    {
        return static_cast<open_flags>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    constexpr open_flags operator&(
        open_flags lhs,
        open_flags rhs
    ) noexcept
    {
        return static_cast<open_flags>(static_cast<int>(lhs) & static_cast<int>(rhs));
    }

    constexpr function_flags operator|(
        function_flags lhs,
        function_flags rhs
    ) noexcept
    {
        return static_cast<function_flags>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    constexpr function_flags operator&(
        function_flags lhs,
        function_flags rhs
    ) noexcept
    {
        return static_cast<function_flags>(static_cast<int>(lhs) & static_cast<int>(rhs));
    }

    constexpr prepare_flags operator|(
        prepare_flags lhs,
        prepare_flags rhs
    ) noexcept
    {
        return static_cast<prepare_flags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
    }

    constexpr prepare_flags operator&(
        prepare_flags lhs,
        prepare_flags rhs
    ) noexcept
    {
        return static_cast<prepare_flags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
    }

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_FLAGS_HPP
