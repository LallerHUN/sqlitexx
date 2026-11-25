#ifndef SQLITEXX_DETAIL_HOOK_SLOTS_HPP
#define SQLITEXX_DETAIL_HOOK_SLOTS_HPP

/// \file
/// The hooks of one database: SQLite keeps one of each kind per connection without owning it, so the database keeps
/// the control blocks here, on the heap so that their address survives moving the database. Installing a hook
/// releases the one it replaces; clearing releases it too, and the destructor clears them all from SQLite before
/// releasing them, while the connection is still open.

#include "sqlitexx/detail/registration_block.hpp"
#include "sqlitexx/native/hook_callbacks.hpp"

extern "C"
{
    struct sqlite3;
}

namespace sqlitexx
{
namespace detail
{
    class hook_slots final
    {
    public:
        explicit hook_slots(sqlite3* connection) noexcept;

        hook_slots(const hook_slots&) = delete;
        hook_slots& operator=(const hook_slots&) = delete;

        ~hook_slots();

        /// Installing: `block` (with its callable) and `hook` take the place of what was there, which is released
        void install_commit(
            registration_block* block,
            native::commit_callback hook
        ) noexcept;
        void install_rollback(
            registration_block* block,
            native::rollback_callback hook
        ) noexcept;
        void install_update(
            registration_block* block,
            native::update_callback hook
        ) noexcept;
        void install_progress(
            registration_block* block,
            int instructions,
            native::progress_callback handler
        ) noexcept;

        /// Clearing the hook of a kind, and releasing its block. With `only`, only when that block is installed.
        void clear(
            registration_block::kind which,
            const registration_block* only = nullptr
        ) noexcept;

    private:
        sqlite3* handle;
        registration_block* commit = nullptr;
        registration_block* rollback = nullptr;
        registration_block* update = nullptr;
        registration_block* progress = nullptr;

        registration_block*& slot(registration_block::kind which) noexcept;

        /// Puts `block` in the slot of its kind and releases what was there
        void replace(
            registration_block::kind which,
            registration_block* block
        ) noexcept;
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_HOOK_SLOTS_HPP
