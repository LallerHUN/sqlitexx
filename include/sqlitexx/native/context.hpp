#ifndef SQLITEXX_NATIVE_CONTEXT_HPP
#define SQLITEXX_NATIVE_CONTEXT_HPP

#include "sqlitexx/config.hpp"
#include "sqlitexx/native/flags.hpp"
#include "sqlitexx/native/result.hpp"

#include <cstddef>
#include <cstdint>

extern "C"
{
    struct sqlite3_context;
}

namespace sqlitexx
{
namespace native
{
    /// Where an SQL function puts its result (sqlite3_context). A view, valid only during the call. Mirrors
    /// sqlite3_result_* and sqlite3_user_data one to one and never throws.
    class context final
    {
    public:
        explicit context(sqlite3_context* handle) noexcept;

        sqlite3_context* handle() const noexcept;

        /// The user_data pointer given to connection::create_function, create_aggregate or create_window_function
        void* user_data() const noexcept;

        /// Memory for one aggregate's state (sqlite3_aggregate_context): zero-filled on the first call for a group,
        /// the same memory on later calls. With 0 bytes, nothing is allocated: null when no step() asked for memory.
        /// SQLite frees it after the final callback.
        void* aggregate_context(std::size_t bytes) noexcept;

        /// The result. Without a call the result is NULL.
        void result_null() noexcept;
        void result_int(int value) noexcept;
        void result_int64(std::int64_t value) noexcept;
        void result_double(double value) noexcept;
        void result_text(
            string_view value,
            bind_mode mode = bind_mode::copy
        ) noexcept;
        void result_text16(
            u16string_view value,
            bind_mode mode = bind_mode::copy
        ) noexcept;
        void result_blob(
            const void* data,
            std::size_t size,
            bind_mode mode = bind_mode::copy
        ) noexcept;
        result result_zeroblob(std::uint64_t size) noexcept;

        /// The function failed: the statement running it fails with `message` and primary_code::error, or with the code
        /// result_error_code() sets afterwards.
        void result_error(string_view message) noexcept;
        void result_error_code(result code) noexcept;
        void result_error_nomem() noexcept;
        void result_error_toobig() noexcept;

    private:
        sqlite3_context* ctx;
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_CONTEXT_HPP
