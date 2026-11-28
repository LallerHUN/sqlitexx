#ifndef SQLNX_SQLITE_HOOKS_HPP
#define SQLNX_SQLITE_HOOKS_HPP

/// \file
/// Hooks: C++ callables SQLite runs when transactions commit or roll back, when rows change, and while statements run.
///
///     db.update_hook([&](sqlnx::sqlite::native::update_kind kind, sqlnx::sqlite::cstring_view schema,
///                        sqlnx::sqlite::cstring_view table, std::int64_t rowid) { ... });
///     db.commit_hook([&] { return changes_are_valid(); });   // false turns the commit into a rollback
///     db.rollback_hook([&] { ... });
///     db.progress_handler(1000, [&] { return !cancelled; });  // false interrupts the running statement
///
///  - commit_hook: called before a transaction commits, also an automatic one around a single statement. It returns
///    void (the commit goes on) or something that converts to bool: false turns the commit into a rollback, and the
///    statement committing fails with extended_code::constraint_commithook.
///  - rollback_hook: called after a transaction rolls back, by ROLLBACK or by an error. Not when closing the database
///    rolls back what was left open.
///  - update_hook: called for every row inserted, updated or deleted in a rowid table, with the kind of change, the
///    schema ("main", "temp" or an attached database), the table and the row's rowid. Not for WITHOUT ROWID tables,
///    nor for changes that clear a table at once (DELETE without WHERE may do that) or come from conflict resolution.
///  - progress_handler: called every `instructions` virtual machine instructions while a statement runs (at least 1;
///    a few thousand is a usual choice). It returns something that converts to bool: false interrupts the statement,
///    which fails with primary_code::interrupt.
///
/// A connection has one hook of each kind: setting one replaces the one before, which is released at once, and passing
/// nullptr clears it. The database keeps the callable (moved, for an rvalue) until then or until it closes. With a
/// registrations token (sqlnx/sqlite/registrations.hpp), the hook is also cleared when the token ends, unless another
/// one has replaced it; clearing a hook works while statements run, so a token always can.
///
/// A hook must not change the connection that runs it: no statements on it, no closing it, no setting hooks from
/// inside a hook. SQLite gives the hook no way to report an error. With exceptions enabled, an exception from a commit
/// hook turns the commit into a rollback, and one from a progress handler interrupts the statement; an exception from
/// an update or rollback hook ends the program (std::terminate), since it may not unwind through SQLite.

#include "sqlnx/sqlite/database.hpp"
#include "sqlnx/sqlite/detail/hook_call.hpp"
#include "sqlnx/sqlite/detail/out_of_memory.hpp"

#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    template <class F>
    expected<void> database::commit_hook(F hook) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(
            detail::is_yes_no_hook<F, true>::value,
            "a commit hook takes no arguments and returns void, or bool: false turns the commit into a rollback"
        );
        return install_hook<detail::commit_caller<F>>(
            std::move(hook),
            0,
            nullptr,
            "commit_hook() needs an open database"
        );
    }

    template <class F>
    expected<void> database::commit_hook(
        F hook,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(
            detail::is_yes_no_hook<F, true>::value,
            "a commit hook takes no arguments and returns void, or bool: false turns the commit into a rollback"
        );
        return install_hook<detail::commit_caller<F>>(
            std::move(hook),
            0,
            &scope,
            "commit_hook() needs an open database"
        );
    }

    template <class F>
    expected<void> database::rollback_hook(F hook) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(detail::is_hook_callable<F, void()>::value, "a rollback hook takes no arguments");
        return install_hook<detail::rollback_caller<F>>(
            std::move(hook),
            0,
            nullptr,
            "rollback_hook() needs an open database"
        );
    }

    template <class F>
    expected<void> database::rollback_hook(
        F hook,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(detail::is_hook_callable<F, void()>::value, "a rollback hook takes no arguments");
        return install_hook<detail::rollback_caller<F>>(
            std::move(hook),
            0,
            &scope,
            "rollback_hook() needs an open database"
        );
    }

    template <class F>
    expected<void> database::update_hook(F hook) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(
            detail::is_hook_callable<F, detail::update_hook_signature>::value,
            "an update hook takes the kind of change (native::update_kind), the schema and the table (cstring_view) "
            "and "
            "the rowid (std::int64_t)"
        );
        return install_hook<detail::update_caller<F>>(
            std::move(hook),
            0,
            nullptr,
            "update_hook() needs an open database"
        );
    }

    template <class F>
    expected<void> database::update_hook(
        F hook,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(
            detail::is_hook_callable<F, detail::update_hook_signature>::value,
            "an update hook takes the kind of change (native::update_kind), the schema and the table (cstring_view) "
            "and "
            "the rowid (std::int64_t)"
        );
        return install_hook<detail::update_caller<F>>(
            std::move(hook),
            0,
            &scope,
            "update_hook() needs an open database"
        );
    }

    template <class F>
    expected<void> database::progress_handler(
        int instructions,
        F handler
    ) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(
            detail::is_yes_no_hook<F, false>::value,
            "a progress handler takes no arguments and returns bool: false interrupts the statement"
        );
        return install_hook<detail::progress_caller<F>>(
            std::move(handler),
            instructions,
            nullptr,
            "progress_handler() needs an open database"
        );
    }

    template <class F>
    expected<void> database::progress_handler(
        int instructions,
        F handler,
        registrations& scope
    ) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        static_assert(
            detail::is_yes_no_hook<F, false>::value,
            "a progress handler takes no arguments and returns bool: false interrupts the statement"
        );
        return install_hook<detail::progress_caller<F>>(
            std::move(handler),
            instructions,
            &scope,
            "progress_handler() needs an open database"
        );
    }

    template <
        class Caller,
        class F>
    expected<void> database::install_hook(
        F hook,
        int instructions,
        registrations* scope,
        const char* closed_message
    ) noexcept(std::is_nothrow_move_constructible<F>::value)
    {
        using block = detail::scoped_callable<F>;

        if (!db)
        {
            return expected<void>(unexpect, needs_open(closed_message));
        }
        if (Caller::kind == detail::registration_block::kind::progress_handler && instructions < 1)
        {
            return expected<void>(
                unexpect,
                detail::error_with(
                    native::result(static_cast<int>(native::primary_code::range)),
                    "progress_handler() needs at least 1 instruction between calls"
                )
            );
        }
        if (!hooks)
        {
            hooks.reset(new (std::nothrow) detail::hook_slots(db.handle()));
            if (!hooks)
            {
                return expected<void>(unexpect, detail::out_of_memory());
            }
        }
        block* made = block::make_hook(db.handle(), hooks.get(), Caller::kind, std::move(hook));
        if (made == nullptr)
        {
            return expected<void>(unexpect, detail::out_of_memory());
        }

        // Installing releases the hook it replaces; the token, if any, shares the new one from here
        Caller::install(*hooks, made, instructions);
        if (scope != nullptr)
        {
            scope->adopt(made);
        }
        return {};
    }
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_HOOKS_HPP
