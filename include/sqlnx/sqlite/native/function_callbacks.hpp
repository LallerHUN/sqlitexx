#ifndef SQLNX_SQLITE_NATIVE_FUNCTION_CALLBACKS_HPP
#define SQLNX_SQLITE_NATIVE_FUNCTION_CALLBACKS_HPP

extern "C"
{
    struct sqlite3_context;
    struct sqlite3_value;
}

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        /// The callbacks connection::create_function, create_aggregate and create_window_function take, as in
        /// sqlite3_create_function_v2 and sqlite3_create_window_function: the function itself (and an aggregate's or
        /// window function's step and inverse), what gives an aggregate's result (final, and a window function's
        /// current value), and what releases the user_data once SQLite no longer needs it.
        using function_callback = void (*)(
            sqlite3_context* context,
            int argument_count,
            sqlite3_value** arguments
        );
        using final_callback = void (*)(sqlite3_context* context);
        using destroy_callback = void (*)(void* user_data);

    } // native
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_NATIVE_FUNCTION_CALLBACKS_HPP
