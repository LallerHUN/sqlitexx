#ifndef SQLNX_SQLITE_COLLATION_HPP
#define SQLNX_SQLITE_COLLATION_HPP

/// \file
/// Collations written in C++, for COLLATE and ORDER BY:
///
///     db.create_collation("nocase_ascii", [](sqlnx::sqlite::string_view a, sqlnx::sqlite::string_view b) { ... });
///     db.prepare("SELECT name FROM user ORDER BY name COLLATE nocase_ascii") ...
///     db.execute("CREATE TABLE tag(name TEXT COLLATE nocase_ascii UNIQUE)");
///
/// The callable takes two string_views, the texts as UTF-8 and not NUL-terminated, and says how they sort:
///  - A signed integer: negative when the first sorts before the second, zero when they are equal, positive when after,
///    as std::string_view::compare gives.
///  - In C++20, also std::strong_ordering or std::weak_ordering, as operator<=> gives.
///  - A bool doesn't compile: a less-than predicate can't tell SQLite that two texts are equal.
///
/// The order must be consistent: the same two texts always compare the same way, equality is transitive, and a before
/// b means b after a. SQLite relies on that for sorting and for indexes; an index built with an inconsistent collation
/// is corrupt. For the same reason, a collation used by an index must not change once the index exists.
///
/// SQLite can't take an error from a comparison, so the callable can't report one. With exceptions enabled, an
/// exception thrown by the callable ends the program (std::terminate): it may not unwind through SQLite, and carrying
/// on with an unknown order could corrupt an index.
///
/// SQLite keeps a copy of the callable (moved, for an rvalue) until the collation is replaced or removed
/// (database::remove_collation, or a registrations token ending: sqlnx/sqlite/registrations.hpp) or the database
/// closes: a lambda capturing by reference must keep what it refers to alive that long. Registering the same name again
/// replaces it; replacing and removing fail with primary_code::busy while statements are running. Names compare like
/// SQL identifiers, without regard to ASCII case.

#include "sqlnx/sqlite/database.hpp"
#include "sqlnx/sqlite/detail/collation_call.hpp"
#include "sqlnx/sqlite/detail/out_of_memory.hpp"

#include <new>
#include <type_traits>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    template <class Compare>
    expected<void> database::create_collation(
        cstring_view name,
        Compare compare
    ) noexcept(std::is_nothrow_move_constructible<Compare>::value)
    {
        using stored = Compare;
        using caller = detail::collation_caller<stored>;

        static_assert(
            detail::is_collation<stored>::value,
            "a collation is called with two string_views and returns a signed integer (negative, zero or positive), or "
            "in "
            "C++20 std::strong_ordering or std::weak_ordering"
        );

        if (!db)
        {
            return expected<void>(unexpect, needs_open("create_collation() needs an open database"));
        }
        // Running out of memory for SQLite's copy is an error; moving into it is the user's type's work
        stored* owned = new (std::nothrow) stored(std::move(compare));
        if (owned == nullptr)
        {
            return expected<void>(unexpect, detail::out_of_memory());
        }

        // SQLite owns the copy only once registering succeeds: unlike for functions, it doesn't call destroy on failure
        const native::result code = db.create_collation(name.c_str(), owned, &caller::compare, &caller::destroy);
        if (code.failed())
        {
            caller::destroy(owned);
            return expected<void>(unexpect, failure(code));
        }
        return {};
    }

    template <class Compare>
    expected<void> database::create_collation(
        cstring_view name,
        Compare compare,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<Compare>::value)
    {
        using stored = Compare;
        using block = detail::scoped_callable<stored>;
        using caller = detail::collation_caller<stored, detail::shared_with_token<stored>>;

        static_assert(
            detail::is_collation<stored>::value,
            "a collation is called with two string_views and returns a signed integer (negative, zero or positive), or "
            "in "
            "C++20 std::strong_ordering or std::weak_ordering"
        );

        if (!db)
        {
            return expected<void>(unexpect, needs_open("create_collation() needs an open database"));
        }
        block* made =
            block::make(db.handle(), name, 0, detail::registration_block::kind::collation, std::move(compare));
        if (made == nullptr)
        {
            return expected<void>(unexpect, detail::out_of_memory());
        }

        // SQLite doesn't call destroy when registering fails: releasing the block for it lets it go
        const native::result code =
            db.create_collation(name.c_str(), made->user_data(), &caller::compare, &caller::destroy);
        if (code.failed())
        {
            caller::destroy(made->user_data());
            return expected<void>(unexpect, failure(code));
        }
        scope.adopt(made);
        return {};
    }
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_COLLATION_HPP
