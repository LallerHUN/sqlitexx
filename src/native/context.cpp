#include "impl_sqlitexx.hpp"

namespace sqlitexx
{
namespace native
{
    context::context(sqlite3_context* handle) noexcept
        : ctx(handle)
    {
    }

    sqlite3_context* context::handle() const noexcept
    {
        return ctx;
    }

    void* context::user_data() const noexcept
    {
        return sqlite3_user_data(ctx);
    }

    void* context::aggregate_context(std::size_t bytes) noexcept
    {
        const std::size_t longest = 0x7FFFFFFF;
        if (bytes > longest)
        {
            return nullptr;
        }
        return sqlite3_aggregate_context(ctx, static_cast<int>(bytes));
    }

    void context::result_null() noexcept
    {
        sqlite3_result_null(ctx);
    }

    void context::result_int(int value) noexcept
    {
        sqlite3_result_int(ctx, value);
    }

    void context::result_int64(std::int64_t value) noexcept
    {
        sqlite3_result_int64(ctx, static_cast<sqlite3_int64>(value));
    }

    void context::result_double(double value) noexcept
    {
        sqlite3_result_double(ctx, value);
    }

    void context::result_text(
        string_view value,
        bind_mode mode
    ) noexcept
    {
        sqlite3_result_text64(
            ctx,
            non_null(value.data()),
            static_cast<sqlite3_uint64>(value.size()),
            destructor_for(mode),
            SQLITE_UTF8
        );
    }

    void context::result_text16(
        u16string_view value,
        bind_mode mode
    ) noexcept
    {
        sqlite3_result_text64(
            ctx,
            reinterpret_cast<const char*>(non_null(value.data())),
            static_cast<sqlite3_uint64>(value.size() * sizeof(char16_t)),
            destructor_for(mode),
            SQLITE_UTF16
        );
    }

    void context::result_blob(
        const void* data,
        std::size_t size,
        bind_mode mode
    ) noexcept
    {
        // A null pointer would make the result NULL; an empty blob must stay a blob
        static const unsigned char empty = 0;
        sqlite3_result_blob64(
            ctx,
            data == nullptr ? &empty : data,
            static_cast<sqlite3_uint64>(size),
            destructor_for(mode)
        );
    }

    result context::result_zeroblob(std::uint64_t size) noexcept
    {
        return result(sqlite3_result_zeroblob64(ctx, static_cast<sqlite3_uint64>(size)));
    }

    void context::result_error(string_view message) noexcept
    {
        // sqlite3_result_error takes an int length; longer messages are cut short rather than overflowing
        const std::size_t longest = 0x7FFFFFFF;
        const int length = static_cast<int>(message.size() < longest ? message.size() : longest);
        sqlite3_result_error(ctx, non_null(message.data()), length);
    }

    void context::result_error_code(result code) noexcept
    {
        sqlite3_result_error_code(ctx, code.code());
    }

    void context::result_error_nomem() noexcept
    {
        sqlite3_result_error_nomem(ctx);
    }

    void context::result_error_toobig() noexcept
    {
        sqlite3_result_error_toobig(ctx);
    }

} // native
} // sqlitexx
