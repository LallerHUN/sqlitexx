#ifndef SQLITEXX_VALUE_BYTES_HPP
#define SQLITEXX_VALUE_BYTES_HPP

// Blobs as std::byte (C++17) and std::span (C++20), next to sqlitexx::blob:
//
//   std::vector<std::byte>                         binds and reads as a blob, like blob
//   std::span<const std::byte>, std::span<const unsigned char>
//                                                  binds without a copy into a blob first; reading gives a view into
//                                                  SQLite's memory, valid until the next step(), reset() or finalize()
//   std::span<std::byte>, std::span<unsigned char>, fixed extents
//                                                  bind only
//
// Before C++17 this header declares nothing, so it is safe to include in every standard; sqlitexx/value.hpp includes
// it.

#include "sqlitexx/value.hpp"

#include <cstddef>

#if SQLITEXX_CPLUSPLUS >= 202002L && defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#if defined(__cpp_lib_byte)

#include <type_traits>
#include <vector>

namespace sqlitexx
{
template <>
struct value_traits<std::vector<std::byte>>
{
    static native::result bind(
        native::statement& stmt,
        int index,
        const std::vector<std::byte>& value
    ) noexcept
    {
        return stmt.bind_blob(index, value.data(), value.size());
    }

    static expected<std::vector<std::byte>> read(
        const native::statement& stmt,
        int index
    )
    {
        const std::byte* data = static_cast<const std::byte*>(stmt.column_blob(index));
        const std::size_t size = stmt.column_bytes(index);
        if (data == nullptr)
        {
            return std::vector<std::byte>();
        }
        return std::vector<std::byte>(data, data + size);
    }
};

#if defined(__cpp_lib_span)

namespace detail
{
    // The element types a span may have to bind as a blob
    template <class T>
    struct is_blob_byte : std::false_type
    {
    };

    template <>
    struct is_blob_byte<std::byte> : std::true_type
    {
    };

    template <>
    struct is_blob_byte<unsigned char> : std::true_type
    {
    };
} // detail

template <class Byte, std::size_t Extent>
struct value_traits<
    std::span<Byte, Extent>,
    typename std::enable_if<detail::is_blob_byte<std::remove_const_t<Byte>>::value>::type>
{
    using span = std::span<Byte, Extent>;

    static native::result bind(
        native::statement& stmt,
        int index,
        span value
    ) noexcept
    {
        return stmt.bind_blob(index, value.data(), value.size());
    }

    static expected<span> read(
        const native::statement& stmt,
        int index
    )
    {
        static_assert(
            std::is_const_v<Byte> && Extent == std::dynamic_extent,
            "read blobs as std::span<const std::byte> or std::span<const unsigned char>: SQLite's memory is read-only "
            "and the size is only known at run time"
        );
        Byte* data = static_cast<Byte*>(stmt.column_blob(index));
        const std::size_t size = stmt.column_bytes(index);
        if (data == nullptr)
        {
            return span();
        }
        return span(data, size);
    }
};

#endif
} // sqlitexx

#endif

#endif // SQLITEXX_VALUE_BYTES_HPP
