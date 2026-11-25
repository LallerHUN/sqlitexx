#ifndef SQLITEXX_NATIVE_BLOB_HANDLE_HPP
#define SQLITEXX_NATIVE_BLOB_HANDLE_HPP

#include "sqlitexx/native/connection.hpp"
#include "sqlitexx/native/result.hpp"

#include <cstdint>

extern "C"
{
    struct sqlite3_blob;
}

namespace sqlitexx
{
namespace native
{
    // Incremental access to one blob (sqlite3_blob): reading and writing parts of it without loading it whole.
    // Move-only; the destructor closes it. The connection must outlive it.
    //
    // Mirrors the C API one to one: functions return result codes and never throw. Sizes and offsets are ints, as in
    // C, so a blob reached this way holds at most INT_MAX bytes.
    class blob_handle final
    {
    public:
        blob_handle() noexcept = default;

        // Takes ownership of a handle opened elsewhere.
        explicit blob_handle(sqlite3_blob* handle) noexcept;

        blob_handle(blob_handle&& other) noexcept;
        blob_handle& operator=(blob_handle&& other) noexcept;

        blob_handle(const blob_handle&) = delete;
        blob_handle& operator=(const blob_handle&) = delete;

        ~blob_handle();

        // Lifetime. open() closes whatever this held, then opens the value in `column` of the row `rowid` of `table`
        // in `schema` ("main", "temp" or an attached name), for writing when `writable`. When it fails, nothing is
        // held and connection::errmsg() describes why. reopen() moves to another row of the same table and column.
        result open(
            connection& db,
            const char* schema,
            const char* table,
            const char* column,
            std::int64_t rowid,
            bool writable
        ) noexcept;
        result reopen(std::int64_t rowid) noexcept;
        result close() noexcept;
        sqlite3_blob* handle() const noexcept;
        sqlite3_blob* release() noexcept;
        explicit operator bool() const noexcept;

        // Access. A blob's size is fixed: writes cannot grow or shrink it (bind or write a zeroblob of the size needed
        // first). When the row changes through another statement, the handle expires and reads and writes fail with
        // primary_code::abort.
        int bytes() const noexcept;
        result read(
            void* buffer,
            int size,
            int offset
        ) const noexcept;
        result write(
            const void* data,
            int size,
            int offset
        ) noexcept;

    private:
        sqlite3_blob* blob = nullptr;
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_BLOB_HANDLE_HPP
