#ifndef SQLITEXX_NATIVE_FUNCTION_CALLBACKS_HPP
#define SQLITEXX_NATIVE_FUNCTION_CALLBACKS_HPP

extern "C"
{
    struct sqlite3_context;
    struct sqlite3_value;
}

namespace sqlitexx
{
namespace native
{
    // The callbacks connection::create_function takes, as in sqlite3_create_function_v2: the function itself, and what
    // releases its user_data once SQLite no longer needs it.
    using function_callback = void (*)(
        sqlite3_context* context,
        int argument_count,
        sqlite3_value** arguments
    );
    using destroy_callback = void (*)(void* user_data);

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_FUNCTION_CALLBACKS_HPP
