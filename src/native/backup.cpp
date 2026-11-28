#include "impl_sqlnx.hpp"

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        backup::backup(sqlite3_backup* handle) noexcept
            : bak(handle)
        {
        }

        backup::backup(backup&& other) noexcept
            : bak(other.release())
        {
        }

        backup& backup::operator=(backup&& other) noexcept
        {
            if (this != &other)
            {
                finish();
                bak = other.release();
            }
            return *this;
        }

        backup::~backup()
        {
            finish();
        }

        result backup::init(
            connection& destination,
            const char* destination_name,
            connection& source,
            const char* source_name
        ) noexcept
        {
            finish();
            bak = sqlite3_backup_init(destination.handle(), destination_name, source.handle(), source_name);
            if (bak == nullptr)
            {
                return destination.extended_errcode();
            }
            return result();
        }

        result backup::finish() noexcept
        {
            if (bak == nullptr)
            {
                return result();
            }
            const int rc = sqlite3_backup_finish(bak);
            bak = nullptr;
            return result(rc);
        }

        sqlite3_backup* backup::handle() const noexcept
        {
            return bak;
        }

        sqlite3_backup* backup::release() noexcept
        {
            sqlite3_backup* handle = bak;
            bak = nullptr;
            return handle;
        }

        backup::operator bool() const noexcept
        {
            return bak != nullptr;
        }

        result backup::step(int pages) noexcept
        {
            return result(sqlite3_backup_step(bak, pages));
        }

        int backup::remaining() const noexcept
        {
            return sqlite3_backup_remaining(bak);
        }

        int backup::pagecount() const noexcept
        {
            return sqlite3_backup_pagecount(bak);
        }

    } // native
} // sqlite
} // sqlnx
