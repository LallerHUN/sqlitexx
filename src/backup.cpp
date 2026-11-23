#include "impl_sqlitexx.hpp"

#include <utility>

namespace sqlitexx
{
backup::backup(
    native::backup&& started,
    sqlite3* destination_db
) noexcept
    : bak(std::move(started)),
      destination(destination_db)
{
}

expected<bool> backup::step(int pages)
{
    if (!bak)
    {
        return expected<bool>(
            unexpect,
            error(native::result(SQLITE_MISUSE), "step() needs a backup from database::begin_backup_to()")
        );
    }
    const native::result code = bak.step(pages);
    if (code.done())
    {
        return true;
    }
    if (code.ok())
    {
        return false;
    }
    return expected<bool>(unexpect, detail::make_error(destination, code));
}

int backup::remaining() const noexcept
{
    return bak ? bak.remaining() : 0;
}

int backup::page_count() const noexcept
{
    return bak ? bak.pagecount() : 0;
}

native::backup& backup::native_backup() noexcept
{
    return bak;
}

} // sqlitexx
