#include "impl_sqlitexx.hpp"

namespace sqlitexx
{
namespace native
{
    blob_handle::blob_handle(sqlite3_blob* handle) noexcept
        : blob(handle)
    {
    }

    blob_handle::blob_handle(blob_handle&& other) noexcept
        : blob(other.release())
    {
    }

    blob_handle& blob_handle::operator=(blob_handle&& other) noexcept
    {
        if (this != &other)
        {
            close();
            blob = other.release();
        }
        return *this;
    }

    blob_handle::~blob_handle()
    {
        close();
    }

    result blob_handle::open(
        connection& db,
        const char* schema,
        const char* table,
        const char* column,
        std::int64_t rowid,
        bool writable
    ) noexcept
    {
        close();
        const int rc = sqlite3_blob_open(
            db.handle(),
            schema,
            table,
            column,
            static_cast<sqlite3_int64>(rowid),
            writable ? 1 : 0,
            &blob
        );
        if (rc != SQLITE_OK)
        {
            // SQLite sets the handle to NULL on failure; make sure of it
            blob = nullptr;
        }
        return result(rc);
    }

    result blob_handle::reopen(std::int64_t rowid) noexcept
    {
        return result(sqlite3_blob_reopen(blob, static_cast<sqlite3_int64>(rowid)));
    }

    result blob_handle::close() noexcept
    {
        if (blob == nullptr)
        {
            return result();
        }
        const int rc = sqlite3_blob_close(blob);
        blob = nullptr;
        return result(rc);
    }

    sqlite3_blob* blob_handle::handle() const noexcept
    {
        return blob;
    }

    sqlite3_blob* blob_handle::release() noexcept
    {
        sqlite3_blob* handle = blob;
        blob = nullptr;
        return handle;
    }

    blob_handle::operator bool() const noexcept
    {
        return blob != nullptr;
    }

    int blob_handle::bytes() const noexcept
    {
        return sqlite3_blob_bytes(blob);
    }

    result blob_handle::read(
        void* buffer,
        int size,
        int offset
    ) const noexcept
    {
        return result(sqlite3_blob_read(blob, buffer, size, offset));
    }

    result blob_handle::write(
        const void* data,
        int size,
        int offset
    ) noexcept
    {
        return result(sqlite3_blob_write(blob, data, size, offset));
    }

} // native
} // sqlitexx
