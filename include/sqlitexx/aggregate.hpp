#ifndef SQLITEXX_AGGREGATE_HPP
#define SQLITEXX_AGGREGATE_HPP

// Aggregate functions written in C++, used like SUM() or GROUP_CONCAT():
//
//     struct joined
//     {
//         std::string separator;
//         std::string text;
//
//         void step(const std::string& value)
//         {
//             text += text.empty() ? value : separator + value;
//         }
//
//         std::string finish() const
//         {
//             return text;
//         }
//     };
//
//     db.create_aggregate("joined", joined{", ", ""});
//     db.prepare("SELECT team, joined(name) FROM player GROUP BY team") ...
//
// The class has a step(...) member and a finish() member:
//  - SQLite works on a copy of the object passed to create_aggregate, the prototype: a fresh copy per group (per
//    query without GROUP BY), so the prototype can carry settings such as the separator above. The class must be
//    copyable for that; SQL functions (sqlitexx/function.hpp) may be move-only.
//  - step() runs for every row of the group. Its parameters are read like the arguments of an SQL function (see
//    sqlitexx/function.hpp): NULL only into optional<T>. It returns void, or expected<void> to fail the query.
//  - finish() gives the group's result, like an SQL function's return value: optional<T> for NULL, expected<T> to
//    fail. For an empty table, finish() runs on a fresh copy that never saw a row, as COUNT() gives 0.
//  - With exceptions enabled, an exception from either fails the query too, and never reaches SQLite.
//
// A class that also has inverse(...) (the same parameters as step(), taking a row back out) and value() (the result so
// far, without finishing) is a window function as well, usable with OVER:
//
//     SELECT running(amount) OVER (ORDER BY day ROWS BETWEEN 6 PRECEDING AND CURRENT ROW) FROM sales
//
// Without them, using the aggregate with OVER is an error that SQLite reports when preparing the statement.

#include "sqlitexx/database.hpp"
#include "sqlitexx/detail/aggregate_call.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"

#include <new>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
template <class Aggregate>
expected<void> database::create_aggregate(
    cstring_view name,
    Aggregate prototype,
    native::function_flags flags
) noexcept(std::is_nothrow_move_constructible<Aggregate>::value)
{
    using stored = Aggregate;
    static_assert(
        detail::is_aggregate_class<stored>::value,
        "an aggregate class has a step(...) member for each row and a finish() member for the result"
    );
    static_assert(
        std::is_copy_constructible<stored>::value,
        "an aggregate class must be copyable: every group works on its own copy of the prototype"
    );
    using caller = detail::aggregate_caller<stored>;

    if (!db)
    {
        return expected<void>(
            unexpect,
            detail::error_with(
                native::result(static_cast<int>(native::primary_code::misuse)),
                "create_aggregate() needs an open database"
            )
        );
    }
    // Running out of memory for SQLite's copy is an error; moving into it is the user's type's work
    stored* owned = new (std::nothrow) stored(std::move(prototype));
    if (owned == nullptr)
    {
        return expected<void>(unexpect, detail::out_of_memory());
    }

    // From here SQLite owns the prototype: it calls destroy when it is done with it, also when registering fails
    const native::result code = register_aggregate<caller>(name, flags, owned, detail::is_window_class<stored>());
    if (code.failed())
    {
        return expected<void>(unexpect, failure(code));
    }
    return {};
}

template <class Caller>
native::result database::register_aggregate(
    cstring_view name,
    native::function_flags flags,
    void* prototype,
    std::false_type /*window*/
)
{
    return db.create_aggregate(
        name.c_str(),
        Caller::argument_count,
        flags,
        prototype,
        &Caller::step,
        &Caller::finish,
        &Caller::destroy
    );
}

template <class Caller>
native::result database::register_aggregate(
    cstring_view name,
    native::function_flags flags,
    void* prototype,
    std::true_type /*window*/
)
{
    return db.create_window_function(
        name.c_str(),
        Caller::argument_count,
        flags,
        prototype,
        &Caller::step,
        &Caller::finish,
        &Caller::value,
        &Caller::inverse,
        &Caller::destroy
    );
}
} // sqlitexx

#endif // SQLITEXX_AGGREGATE_HPP
