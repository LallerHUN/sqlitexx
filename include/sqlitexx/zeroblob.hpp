#ifndef SQLITEXX_ZEROBLOB_HPP
#define SQLITEXX_ZEROBLOB_HPP

// A blob of `size` zero bytes, without building it in memory first: SQLite reserves the space. Binding only, as a
// parameter or an SQL function result. It is how a blob gets the size that a blob_stream then fills in:
//
//     insert.execute(sqlitexx::zeroblob{1024 * 1024});

#include "sqlitexx/native/result.hpp"
#include "sqlitexx/value.hpp"

#include <cstdint>

namespace sqlitexx
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

} // sqlitexx

#endif // SQLITEXX_ZEROBLOB_HPP
