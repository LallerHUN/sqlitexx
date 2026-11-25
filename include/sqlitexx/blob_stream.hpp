#ifndef SQLITEXX_BLOB_STREAM_HPP
#define SQLITEXX_BLOB_STREAM_HPP

// Reading and writing a large blob in pieces, without holding it in memory whole (SQLite's incremental blob I/O):
//
//     // Reserve the space: a blob's size cannot change through a stream
//     db.prepare("INSERT INTO file(data) VALUES(?)").and_then(execute_with(zeroblob{size}));
//     expected<blob_stream> out = db.open_blob("file", "data", db.last_insert_rowid(), blob_access::read_write);
//     while (out && more) out->write(chunk.data(), chunk.size());
//
//     expected<blob_stream> in = db.open_blob("file", "data", rowid);
//     unsigned char buffer[4096];
//     for (;;)
//     {
//         expected<std::size_t> got = in->read(buffer, sizeof buffer);
//         if (!got || *got == 0) break;   // 0: the end of the blob
//         use(buffer, *got);
//     }
//
// The stream has a position, like a file: read() and write() continue from it and seek() moves it. When the row
// changes through another statement, the stream expires: reads and writes fail with primary_code::abort.

#include "sqlitexx/config.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/blob_handle.hpp"

#include <cstddef>
#include <cstdint>

extern "C"
{
    struct sqlite3;
}

namespace sqlitexx
{
class database;

// How database::open_blob opens a blob
enum class blob_access
{
    read_only,
    read_write,
};

// An open blob, from database::open_blob. Move-only; the destructor closes it. Its database must outlive it.
class blob_stream final
{
public:
    blob_stream() noexcept = default;

    blob_stream(blob_stream&&) noexcept = default;
    blob_stream& operator=(blob_stream&&) noexcept = default;

    blob_stream(const blob_stream&) = delete;
    blob_stream& operator=(const blob_stream&) = delete;

    // Size and position. The size is fixed while the blob is open.
    std::size_t size() const noexcept;
    std::size_t position() const noexcept;

    // Moves the position; past the end is a primary_code::range error and leaves the position as it was.
    expected<void> seek(std::size_t offset);

    // Reads up to `count` bytes from the position into `buffer` and moves past them. Gives the number of bytes read:
    // fewer than `count` near the end, 0 at the end.
    expected<std::size_t> read(
        void* buffer,
        std::size_t count
    );

    // Writes `count` bytes at the position and moves past them. Writing past the end is a primary_code::range error
    // and writes nothing: the blob keeps its size. A stream opened read-only gives primary_code::readonly.
    expected<void> write(
        const void* data,
        std::size_t count
    );

    // Moves to the same column of another row, much faster than opening again. The position goes back to 0.
    expected<void> reopen(std::int64_t rowid);

    // Closes the blob now and reports any error; the destructor closes it too, without a report.
    expected<void> close();

    // The native blob underneath, for what the typed layer does not cover
    native::blob_handle& native_blob() noexcept;

private:
    native::blob_handle blob;
    sqlite3* db = nullptr;
    std::size_t length = 0;
    std::size_t offset = 0;

    friend class database;

    blob_stream(
        native::blob_handle&& opened,
        sqlite3* connection
    ) noexcept;

    error failure(native::result code) const;
    error not_open() const;
};

} // sqlitexx

#endif // SQLITEXX_BLOB_STREAM_HPP
