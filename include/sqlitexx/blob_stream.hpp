#ifndef SQLITEXX_BLOB_STREAM_HPP
#define SQLITEXX_BLOB_STREAM_HPP

// Reading and writing a large blob in pieces, without holding it in memory whole (SQLite's incremental blob I/O):
//
//     // Reserve the space: a blob's size cannot change through a stream
//     db.prepare("INSERT INTO file(data) VALUES(?)").and_then(execute_with(zeroblob{size}));
//     expected<blob_stream> out = db.open_blob("file", "data", db.last_insert_rowid(), blob_access::read_write);
//     while (out && more) out->write(chunk);              // any contiguous container of bytes
//
//     expected<blob_stream> in = db.open_blob("file", "data", rowid);
//     std::vector<unsigned char> all;
//     in->read(all);                                       // the rest of the blob; all.size() is what was read
//
//     std::array<unsigned char, 4096> buffer;              // or without allocating, in pieces
//     for (;;)
//     {
//         expected<std::size_t> got = in->read(buffer);    // fills up to buffer.size()
//         if (!got || *got == 0) break;                    // 0: the end of the blob
//         use(buffer.data(), *got);
//     }
//
// Containers hold bytes: char, signed char, unsigned char, std::byte (C++17) or char8_t (C++20). Reading into a
// container that can grow (std::vector, std::string) sizes it to what was read; reading into one that can't
// (std::array, std::span, a C array) fills it up to its size. Reads give the number of bytes read. Each form also
// takes a `count`: for read(), the most bytes to read; for write(), the first `count` bytes of the container. A count
// larger than the container is a primary_code::range error. The pointer forms remain for memory that isn't in a
// container.
//
// The stream has a position, like a file: read() and write() continue from it and seek() moves it. When the row
// changes through another statement, the stream expires: reads and writes fail with primary_code::abort.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/byte_container.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/blob_handle.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

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

    // Reads into a container that grows to fit (std::vector, std::string): the rest of the blob, or with `count`, at
    // most that many bytes. Gives the number of bytes read; the container ends up that size, and empty on an error.
    template <
        class Bytes,
        typename std::enable_if<
            detail::is_growable_bytes<Bytes>::value,
            int>::type = 0>
    expected<std::size_t> read(Bytes& out)
    {
        return read_growing(out, length - offset);
    }

    template <
        class Bytes,
        typename std::enable_if<
            detail::is_growable_bytes<Bytes>::value,
            int>::type = 0>
    expected<std::size_t> read(
        Bytes& out,
        std::size_t count
    )
    {
        return read_growing(out, count);
    }

    // Reads into a fixed-size buffer (std::array, std::span, a C array) without allocating: up to its size, or with
    // `count`, at most that many bytes, where `count` must not be larger than the buffer. Gives the number of bytes
    // read.
    template <
        class Bytes,
        typename std::enable_if<
            detail::is_fixed_bytes<Bytes>::value,
            int>::type = 0>
    expected<std::size_t> read(Bytes& out)
    {
        return read(static_cast<void*>(out.data()), out.size());
    }

    template <
        class Bytes,
        typename std::enable_if<
            detail::is_fixed_bytes<Bytes>::value,
            int>::type = 0>
    expected<std::size_t> read(
        Bytes& out,
        std::size_t count
    )
    {
        if (count > out.size())
        {
            return expected<std::size_t>(unexpect, too_many(count, out.size()));
        }
        return read(static_cast<void*>(out.data()), count);
    }

    template <
        class Byte,
        std::size_t N,
        typename std::enable_if<
            detail::is_byte<Byte>::value,
            int>::type = 0>
    expected<std::size_t> read(Byte (&out)[N])
    {
        return read(static_cast<void*>(out), N);
    }

    template <
        class Byte,
        std::size_t N,
        typename std::enable_if<
            detail::is_byte<Byte>::value,
            int>::type = 0>
    expected<std::size_t> read(
        Byte (&out)[N],
        std::size_t count
    )
    {
        if (count > N)
        {
            return expected<std::size_t>(unexpect, too_many(count, N));
        }
        return read(static_cast<void*>(out), count);
    }

    // Writes `count` bytes at the position and moves past them. Writing past the end is a primary_code::range error
    // and writes nothing: the blob keeps its size. A stream opened read-only gives primary_code::readonly.
    expected<void> write(
        const void* data,
        std::size_t count
    );

    // Writes a contiguous container of bytes (std::vector, std::string, std::array, std::span, string_view, ...): all
    // of it, or its first `count` bytes. C arrays work too, except arrays of char and char8_t: a string literal would
    // write its terminating zero as well, so pass a string_view for text.
    template <
        class Bytes,
        typename std::enable_if<
            detail::is_readable_bytes<Bytes>::value,
            int>::type = 0>
    expected<void> write(const Bytes& data)
    {
        return write(static_cast<const void*>(data.data()), data.size());
    }

    template <
        class Bytes,
        typename std::enable_if<
            detail::is_readable_bytes<Bytes>::value,
            int>::type = 0>
    expected<void> write(
        const Bytes& data,
        std::size_t count
    )
    {
        if (count > data.size())
        {
            return expected<void>(unexpect, too_many(count, data.size()));
        }
        return write(static_cast<const void*>(data.data()), count);
    }

    template <
        class Byte,
        std::size_t N,
        typename std::enable_if<
            detail::is_byte_array_element<Byte>::value,
            int>::type = 0>
    expected<void> write(const Byte (&data)[N])
    {
        return write(static_cast<const void*>(data), N);
    }

    template <
        class Byte,
        std::size_t N,
        typename std::enable_if<
            detail::is_byte_array_element<Byte>::value,
            int>::type = 0>
    expected<void> write(
        const Byte (&data)[N],
        std::size_t count
    )
    {
        if (count > N)
        {
            return expected<void>(unexpect, too_many(count, N));
        }
        return write(static_cast<const void*>(data), count);
    }

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
    error too_many(
        std::size_t count,
        std::size_t available
    ) const;

    template <class Bytes>
    expected<std::size_t> read_growing(
        Bytes& out,
        std::size_t count
    )
    {
        if (!blob)
        {
            out.resize(0);
            return expected<std::size_t>(unexpect, not_open());
        }
        const std::size_t left = length - offset;
        const std::size_t wanted = count < left ? count : left;
        out.resize(wanted);
        if (wanted == 0)
        {
            return std::size_t(0);
        }
        expected<std::size_t> got = read(static_cast<void*>(&out[0]), wanted);
        if (!got)
        {
            out.resize(0);
        }
        return got;
    }
};

} // sqlitexx

#endif // SQLITEXX_BLOB_STREAM_HPP
