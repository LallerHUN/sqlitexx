#ifndef SQLITEXX_NATIVE_COLLATION_CALLBACK_HPP
#define SQLITEXX_NATIVE_COLLATION_CALLBACK_HPP

namespace sqlitexx
{
namespace native
{
    // The comparison connection::create_collation takes, as in sqlite3_create_collation_v2: the user_data, then each
    // text as its size in bytes and a pointer to its first byte (not NUL-terminated). It returns a negative number,
    // zero or a positive number when the first text sorts before, equal to or after the second.
    using compare_callback = int (*)(
        void* user_data,
        int first_size,
        const void* first,
        int second_size,
        const void* second
    );

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_COLLATION_CALLBACK_HPP
