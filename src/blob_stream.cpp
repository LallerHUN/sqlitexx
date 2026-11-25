#include "impl_sqlitexx.hpp"

#include <string>
#include <utility>

namespace sqlitexx
{
blob_stream::blob_stream(
    native::blob_handle&& opened,
    sqlite3* connection
) noexcept
    : blob(std::move(opened)),
      db(connection),
      length(static_cast<std::size_t>(blob.bytes()))
{
}

std::size_t blob_stream::size() const noexcept
{
    return length;
}

std::size_t blob_stream::position() const noexcept
{
    return offset;
}

expected<void> blob_stream::seek(std::size_t to) noexcept
{
    if (!blob)
    {
        return expected<void>(unexpect, not_open());
    }
    if (to > length)
    {
        return expected<void>(
            unexpect,
            detail::built_error(
                native::result(SQLITE_RANGE),
                [&]
                {
                    return "cannot seek to " + std::to_string(to) + " in a blob of " + std::to_string(length)
                        + " bytes";
                }
            )
        );
    }
    offset = to;
    return {};
}

expected<std::size_t> blob_stream::read(
    void* buffer,
    std::size_t count
) noexcept
{
    if (!blob)
    {
        return expected<std::size_t>(unexpect, not_open());
    }
    // The size fits an int (SQLite gives it as one), and so does anything up to it
    const std::size_t left = length - offset;
    const std::size_t wanted = count < left ? count : left;
    if (wanted == 0)
    {
        return std::size_t(0);
    }
    const native::result code = blob.read(buffer, static_cast<int>(wanted), static_cast<int>(offset));
    if (code.failed())
    {
        return expected<std::size_t>(unexpect, failure(code));
    }
    offset += wanted;
    return wanted;
}

expected<void> blob_stream::write(
    const void* data,
    std::size_t count
) noexcept
{
    if (!blob)
    {
        return expected<void>(unexpect, not_open());
    }
    if (count > length - offset)
    {
        return expected<void>(
            unexpect,
            detail::built_error(
                native::result(SQLITE_RANGE),
                [&]
                {
                    return "writing " + std::to_string(count) + " bytes at " + std::to_string(offset)
                        + " goes past the end of a " + std::to_string(length)
                        + "-byte blob; a blob's size cannot change through a stream";
                }
            )
        );
    }
    if (count == 0)
    {
        return {};
    }
    const native::result code = blob.write(data, static_cast<int>(count), static_cast<int>(offset));
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    offset += count;
    return {};
}

expected<void> blob_stream::reopen(std::int64_t rowid) noexcept
{
    if (!blob)
    {
        return expected<void>(unexpect, not_open());
    }
    const native::result code = blob.reopen(rowid);
    if (code.failed())
    {
        // SQLite keeps the handle but it can no longer be used; only closing is left
        length = 0;
        offset = 0;
        return expected<void>(unexpect, failure(code));
    }
    length = static_cast<std::size_t>(blob.bytes());
    offset = 0;
    return {};
}

expected<void> blob_stream::close() noexcept
{
    const native::result code = blob.close();
    length = 0;
    offset = 0;
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    return {};
}

native::blob_handle& blob_stream::native_blob() noexcept
{
    return blob;
}

error blob_stream::failure(native::result code) const noexcept
{
    return detail::make_error(db, code);
}

error blob_stream::too_many(
    std::size_t count,
    std::size_t available
) const noexcept
{
    return detail::built_error(
        native::result(SQLITE_RANGE),
        [&]
        {
            return "a count of " + std::to_string(count) + " bytes is more than the container's "
                + std::to_string(available);
        }
    );
}

error blob_stream::not_open() const noexcept
{
    return detail::error_with(native::result(SQLITE_MISUSE), "the blob stream is not open");
}

} // sqlitexx
