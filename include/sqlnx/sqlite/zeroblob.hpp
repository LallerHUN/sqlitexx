#ifndef SQLNX_SQLITE_ZEROBLOB_HPP
#define SQLNX_SQLITE_ZEROBLOB_HPP

/// \file
/// A blob of `size` zero bytes, without building it in memory first: SQLite reserves the space. Binding only, as a
/// parameter or an SQL function result. It is how a blob gets the size that a blob_stream then fills in:
///
///     insert.execute(sqlnx::sqlite::zeroblob{1024 * 1024});

#include "sqlnx/sqlite/native/result.hpp"
#include "sqlnx/sqlite/value.hpp"

#include <cstdint>

namespace sqlnx
{
namespace sqlite
{
    struct zeroblob
    {
        std::uint64_t size;
    };

    template <>
    struct value_traits<zeroblob>
    {
        template <class Target>
        static native::result bind(
            Target& stmt,
            int index,
            const zeroblob& value
        ) noexcept
        {
            return stmt.bind_zeroblob(index, value.size);
        }
    };

} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_ZEROBLOB_HPP
