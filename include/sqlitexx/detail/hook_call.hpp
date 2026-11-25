#ifndef SQLITEXX_DETAIL_HOOK_CALL_HPP
#define SQLITEXX_DETAIL_HOOK_CALL_HPP

// What SQLite calls for hooks made from C++ callables. Hooks can't report errors: a commit hook can only turn the
// commit into a rollback and a progress handler can only interrupt, so with exceptions enabled an exception from them
// does that. An exception from an update or rollback hook ends the program (std::terminate), as nothing may unwind
// through SQLite and there is no way to stop what it reports.

#include "sqlitexx/config.hpp"
#include "sqlitexx/cstring_view.hpp"
#include "sqlitexx/detail/hook_slots.hpp"
#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/detail/registration_block.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/native/hook_callbacks.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    // What a callable gives when called with `Args`; no `type` when it can't be called that way
    template <class F, class Signature, class = void>
    struct hook_result
    {
    };

    template <class F, class... Args>
    struct hook_result<
        F,
        void(Args...),
        typename make_void<decltype(std::declval<F&>()(std::declval<Args>()...))>::type>
    {
        using type = decltype(std::declval<F&>()(std::declval<Args>()...));
    };

    // Called with no arguments, giving `void` (when `void_allowed`) or something that converts to bool
    template <class F, bool void_allowed, class = void>
    struct is_yes_no_hook : std::false_type
    {
    };

    template <class F, bool void_allowed>
    struct is_yes_no_hook<F, void_allowed, typename make_void<typename hook_result<F, void()>::type>::type>
        : std::integral_constant<
              bool,
              (void_allowed && std::is_void<typename hook_result<F, void()>::type>::value)
                  || std::is_constructible<bool, typename hook_result<F, void()>::type>::value>
    {
    };

    template <class F, class Signature, class = void>
    struct is_hook_callable : std::false_type
    {
    };

    template <class F, class Signature>
    struct is_hook_callable<F, Signature, typename make_void<typename hook_result<F, Signature>::type>::type>
        : std::true_type
    {
    };

    using update_hook_signature = void(
        native::update_kind,
        cstring_view,
        cstring_view,
        std::int64_t
    );

    // Whether a yes-or-no hook says yes: a void hook always does
    template <class F>
    bool says_yes(
        F& hook,
        std::true_type /*void*/
    )
    {
        hook();
        return true;
    }

    template <class F>
    bool says_yes(
        F& hook,
        std::false_type /*void*/
    )
    {
        return static_cast<bool>(hook());
    }

    // SQLite's answer for a yes-or-no hook: 0 goes on, 1 stops (rolls back, or interrupts). An exception stops.
    template <class F>
    int answer(void* user_data) noexcept
    {
        F* hook = shared_with_token<F>::callable(user_data);
        if (hook == nullptr)
        {
            return 0;
        }
        using is_void = std::is_void<typename hook_result<F, void()>::type>;
#if SQLITEXX_DETAIL_EXCEPTIONS
        try
        {
            return says_yes(*hook, is_void()) ? 0 : 1;
        }
        catch (...)
        {
            return 1;
        }
#else
        return says_yes(*hook, is_void()) ? 0 : 1;
#endif
    }

    template <class F>
    struct commit_caller
    {
        static const registration_block::kind kind = registration_block::kind::commit_hook;

        static int call(void* user_data) noexcept
        {
            return answer<F>(user_data);
        }

        static void install(
            hook_slots& slots,
            registration_block* block,
            int
        ) noexcept
        {
            slots.install_commit(block, &call);
        }
    };

    template <class F>
    struct rollback_caller
    {
        static const registration_block::kind kind = registration_block::kind::rollback_hook;

        static void call(void* user_data) noexcept
        {
            F* hook = shared_with_token<F>::callable(user_data);
            if (hook != nullptr)
            {
                (*hook)();
            }
        }

        static void install(
            hook_slots& slots,
            registration_block* block,
            int
        ) noexcept
        {
            slots.install_rollback(block, &call);
        }
    };

    template <class F>
    struct update_caller
    {
        static const registration_block::kind kind = registration_block::kind::update_hook;

        static void call(
            void* user_data,
            int change,
            const char* schema,
            const char* table,
            long long rowid
        ) noexcept
        {
            F* hook = shared_with_token<F>::callable(user_data);
            if (hook != nullptr)
            {
                (*hook)(
                    static_cast<native::update_kind>(change),
                    cstring_view(schema),
                    cstring_view(table),
                    static_cast<std::int64_t>(rowid)
                );
            }
        }

        static void install(
            hook_slots& slots,
            registration_block* block,
            int
        ) noexcept
        {
            slots.install_update(block, &call);
        }
    };

    template <class F>
    struct progress_caller
    {
        static const registration_block::kind kind = registration_block::kind::progress_handler;

        static int call(void* user_data) noexcept
        {
            return answer<F>(user_data);
        }

        static void install(
            hook_slots& slots,
            registration_block* block,
            int instructions
        ) noexcept
        {
            slots.install_progress(block, instructions, &call);
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_HOOK_CALL_HPP
