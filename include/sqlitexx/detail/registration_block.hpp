#ifndef SQLITEXX_DETAIL_REGISTRATION_BLOCK_HPP
#define SQLITEXX_DETAIL_REGISTRATION_BLOCK_HPP

/// \file
/// The control block of a function, aggregate or collation registered with a registrations token, and of every hook:
/// what SQLite gets as user_data, holding the callable, shared by SQLite and the token. Either side may let go first:
/// SQLite when the registration is replaced, removed or the database closes, the token when it removes, releases or
/// ends. The block goes when both have let go. For hooks, SQLite owns nothing: the database's hook_slots take its side.
///
/// The callable ends when SQLite lets go. When the token ends without having removed the registration (a statement was
/// running), it ends the callable itself: callable() is null from then on, and what SQLite calls fails (functions) or
/// ends the program (collations).

#include "sqlitexx/cstring_view.hpp"
#include "sqlitexx/native/result.hpp"

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

extern "C"
{
    struct sqlite3;
}

namespace sqlitexx
{
class registrations;

namespace detail
{
    class hook_slots;

    class registration_block
    {
    public:
        enum class kind
        {
            function,
            collation,
            commit_hook,
            rollback_hook,
            update_hook,
            progress_handler,
        };

        registration_block(const registration_block&) = delete;
        registration_block& operator=(const registration_block&) = delete;

        /// SQLite's destroy callback: SQLite no longer refers to the block. For hooks, the hook_slots call it.
        static void released_by_sqlite(void* block) noexcept;

    protected:
        /// `slots` for hooks, null for functions and collations
        registration_block(
            sqlite3* connection,
            std::unique_ptr<char[]> registered_name,
            int arguments,
            kind registered,
            hook_slots* slots
        ) noexcept;

        virtual ~registration_block();

        /// The name as SQLite needs it: NUL-terminated. Null when copying it ran out of memory.
        static std::unique_ptr<char[]> copy_name(cstring_view name) noexcept;

        virtual void end_callable() noexcept = 0;

    private:
        friend class sqlitexx::registrations;

        sqlite3* handle;
        std::unique_ptr<char[]> name;
        int argument_count;
        kind what;
        hook_slots* hooks;
        bool held_by_sqlite = true;
        bool held_by_token = false;
        registration_block* next = nullptr;

        /// For the token: removes the registration from SQLite, which then releases the block. Ok, doing nothing,
        /// when SQLite has released it already.
        native::result remove() noexcept;

        /// The token lets go; the block goes when SQLite has let go too
        void released_by_token() noexcept;
    };

    template <class F>
    class scoped_callable final : public registration_block
    {
    public:
        /// Null when out of memory. Moving the callable in is the user's type's work: what it throws passes through.
        static scoped_callable* make(
            sqlite3* connection,
            cstring_view name,
            int arguments,
            kind registered,
            F&& callable
        ) noexcept(std::is_nothrow_move_constructible<F>::value)
        {
            std::unique_ptr<char[]> copied = copy_name(name);
            if (!copied)
            {
                return nullptr;
            }
            return new (std::nothrow)
                scoped_callable(connection, std::move(copied), arguments, registered, nullptr, std::move(callable));
        }

        /// A hook's block: no name, kept by the database's hook_slots. Null when out of memory.
        static scoped_callable* make_hook(
            sqlite3* connection,
            hook_slots* slots,
            kind registered,
            F&& callable
        ) noexcept(std::is_nothrow_move_constructible<F>::value)
        {
            return new (std::nothrow)
                scoped_callable(connection, std::unique_ptr<char[]>(), 0, registered, slots, std::move(callable));
        }

        /// What SQLite gets as user_data: the block, as the registration_block that released_by_sqlite expects
        void* user_data() noexcept
        {
            return static_cast<registration_block*>(this);
        }

        /// What SQLite calls through user_data. Null once the token has ended the callable.
        static F* callable(void* user_data) noexcept
        {
            return static_cast<scoped_callable*>(static_cast<registration_block*>(user_data))->object;
        }

    private:
        alignas(F) unsigned char storage[sizeof(F)];
        F* object;

        scoped_callable(
            sqlite3* connection,
            std::unique_ptr<char[]> registered_name,
            int arguments,
            kind registered,
            hook_slots* slots,
            F&& callable
        )
            : registration_block(
                  connection,
                  std::move(registered_name),
                  arguments,
                  registered,
                  slots
              ),
              object(new (static_cast<void*>(storage)) F(std::move(callable)))
        {
        }

        ~scoped_callable() override
        {
            end_callable();
        }

        void end_callable() noexcept override
        {
            if (object != nullptr)
            {
                object->~F();
                object = nullptr;
            }
        }
    };

    /// How the callers reach the callable behind user_data: owned by SQLite alone (created without a token), or
    /// through a token's control block
    template <class F>
    struct owned_by_sqlite
    {
        static F* callable(void* user_data) noexcept
        {
            return static_cast<F*>(user_data);
        }

        static void destroy(void* user_data) noexcept
        {
            delete static_cast<F*>(user_data);
        }
    };

    template <class F>
    struct shared_with_token
    {
        static F* callable(void* user_data) noexcept
        {
            return scoped_callable<F>::callable(user_data);
        }

        static void destroy(void* user_data) noexcept
        {
            registration_block::released_by_sqlite(user_data);
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_REGISTRATION_BLOCK_HPP
