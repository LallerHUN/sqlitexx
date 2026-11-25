#ifndef SQLITEXX_FUNCTION_HPP
#define SQLITEXX_FUNCTION_HPP

/// \file
/// SQL functions written in C++:
///
///     db.create_function("greet", [](const std::string& name) { return "Hello, " + name; });
///     db.prepare("SELECT greet(name) FROM user") ...
///
/// The callable is a lambda (without auto parameters), a function object with one operator() or a function pointer.
/// Its parameters and its return type can be anything value_traits reads and binds, as for statements:
///  - Arguments are read like columns. NULL into a parameter that is not an optional<T> fails the call with a
///    primary_code::mismatch error, as does an integer that doesn't fit.
///  - The return value is the result. optional<T> returns NULL when empty, and a function returning void returns NULL.
///  - Returning expected<T> (or expected<void>) reports errors: the statement calling the function fails with the
///    error's code and message.
///  - With exceptions enabled, an exception thrown by the callable fails the call too, with what() as the message;
///    std::bad_alloc becomes primary_code::nomem. Exceptions never reach SQLite.
///
/// SQLite keeps a copy of the callable (moved, for an rvalue) until the function is replaced or removed
/// (database::remove_function, or a registrations token ending: sqlitexx/registrations.hpp) or the database closes. It
/// may be called on any thread that uses the connection, and from inside any statement: a lambda capturing by reference
/// must keep what it refers to alive that long.
///
/// The function takes exactly as many arguments as the callable has parameters; SQLite rejects calls with a different
/// count when the statement is prepared. Registering the same name and count again replaces the function.
///
/// Mark functions native::function_flags::deterministic when the same arguments always give the same result: SQLite
/// can then optimize calls and use them in indexes. Text arrives and leaves as UTF-8.

#include "sqlitexx/database.hpp"
#include "sqlitexx/detail/function_call.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"

#include <new>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
template <class F>
expected<void> database::create_function(
    cstring_view name,
    F function,
    native::function_flags flags
) noexcept(std::is_nothrow_move_constructible<F>::value)
{
    using stored = F;
    using caller = detail::function_caller<stored>;

    if (!db)
    {
        return expected<void>(unexpect, needs_open("create_function() needs an open database"));
    }
    // Running out of memory for SQLite's copy is an error; moving into it is the user's type's work
    stored* owned = new (std::nothrow) stored(std::move(function));
    if (owned == nullptr)
    {
        return expected<void>(unexpect, detail::out_of_memory());
    }

    // From here SQLite owns the copy: it calls destroy when it is done with it, also when registering fails
    const native::result code =
        db.create_function(name.c_str(), caller::argument_count, flags, owned, &caller::call, &caller::destroy);
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    return {};
}

template <class F>
expected<void> database::create_function(
    cstring_view name,
    F function,
    registrations& scope
) noexcept(std::is_nothrow_move_constructible<F>::value)
{
    return create_function(name, std::move(function), native::function_flags::none, scope);
}

template <class F>
expected<void> database::create_function(
    cstring_view name,
    F function,
    native::function_flags flags,
    registrations& scope
) noexcept(std::is_nothrow_move_constructible<F>::value)
{
    using block = detail::scoped_callable<F>;
    using caller = detail::function_caller<F, detail::shared_with_token<F>>;

    if (!db)
    {
        return expected<void>(unexpect, needs_open("create_function() needs an open database"));
    }
    block* made = block::make(
        db.handle(),
        name,
        caller::argument_count,
        detail::registration_block::kind::function,
        std::move(function)
    );
    if (made == nullptr)
    {
        return expected<void>(unexpect, detail::out_of_memory());
    }

    // When registering fails, SQLite calls destroy and the block goes, as the token doesn't hold it yet
    const native::result code = db.create_function(
        name.c_str(),
        caller::argument_count,
        flags,
        made->user_data(),
        &caller::call,
        &caller::destroy
    );
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    scope.adopt(made);
    return {};
}
} // sqlitexx

#endif // SQLITEXX_FUNCTION_HPP
