#include "impl_sqlnx.hpp"

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        value::value(sqlite3_value* handle) noexcept
            : val(handle)
        {
        }

        sqlite3_value* value::handle() const noexcept
        {
            return val;
        }

        value_type value::type() const noexcept
        {
            return static_cast<value_type>(sqlite3_value_type(val));
        }

        int value::to_int() const noexcept
        {
            return sqlite3_value_int(val);
        }

        std::int64_t value::to_int64() const noexcept
        {
            return static_cast<std::int64_t>(sqlite3_value_int64(val));
        }

        double value::to_double() const noexcept
        {
            return sqlite3_value_double(val);
        }

        optional<cstring_view> value::text() const noexcept
        {
            // Pointer first, then the size: reading the text may convert it, which changes the size.
            const unsigned char* text = sqlite3_value_text(val);
            if (text == nullptr)
            {
                return nullopt;
            }
            const int bytes = sqlite3_value_bytes(val);
            return cstring_view::assume_terminated(
                reinterpret_cast<const char*>(text),
                static_cast<std::size_t>(bytes)
            );
        }

        optional<u16cstring_view> value::text16() const noexcept
        {
            const void* text = sqlite3_value_text16(val);
            if (text == nullptr)
            {
                return nullopt;
            }
            const int bytes = sqlite3_value_bytes16(val);
            return u16cstring_view::assume_terminated(
                static_cast<const char16_t*>(text),
                static_cast<std::size_t>(bytes) / sizeof(char16_t)
            );
        }

        const void* value::blob() const noexcept
        {
            return sqlite3_value_blob(val);
        }

        std::size_t value::bytes() const noexcept
        {
            return static_cast<std::size_t>(sqlite3_value_bytes(val));
        }

    } // native
} // sqlite
} // sqlnx
