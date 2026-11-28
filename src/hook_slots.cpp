#include "impl_sqlnx.hpp"

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        hook_slots::hook_slots(sqlite3* connection) noexcept
            : handle(connection)
        {
        }

        hook_slots::~hook_slots()
        {
            clear(registration_block::kind::commit_hook);
            clear(registration_block::kind::rollback_hook);
            clear(registration_block::kind::update_hook);
            clear(registration_block::kind::progress_handler);
        }

        void hook_slots::install_commit(
            registration_block* block,
            native::commit_callback hook
        ) noexcept
        {
            sqlite3_commit_hook(handle, hook, block);
            replace(registration_block::kind::commit_hook, block);
        }

        void hook_slots::install_rollback(
            registration_block* block,
            native::rollback_callback hook
        ) noexcept
        {
            sqlite3_rollback_hook(handle, hook, block);
            replace(registration_block::kind::rollback_hook, block);
        }

        void hook_slots::install_update(
            registration_block* block,
            native::update_callback hook
        ) noexcept
        {
            sqlite3_update_hook(handle, hook, block);
            replace(registration_block::kind::update_hook, block);
        }

        void hook_slots::install_progress(
            registration_block* block,
            int instructions,
            native::progress_callback handler
        ) noexcept
        {
            sqlite3_progress_handler(handle, instructions, handler, block);
            replace(registration_block::kind::progress_handler, block);
        }

        void hook_slots::clear(
            registration_block::kind which,
            const registration_block* only
        ) noexcept
        {
            registration_block* installed = slot(which);
            if (installed == nullptr || (only != nullptr && installed != only))
            {
                return;
            }
            switch (which)
            {
            case registration_block::kind::commit_hook:
                sqlite3_commit_hook(handle, nullptr, nullptr);
                break;
            case registration_block::kind::rollback_hook:
                sqlite3_rollback_hook(handle, nullptr, nullptr);
                break;
            case registration_block::kind::update_hook:
                sqlite3_update_hook(handle, nullptr, nullptr);
                break;
            case registration_block::kind::progress_handler:
                sqlite3_progress_handler(handle, 0, nullptr, nullptr);
                break;
            case registration_block::kind::function:
            case registration_block::kind::collation:
                return;
            }
            replace(which, nullptr);
        }

        registration_block*& hook_slots::slot(registration_block::kind which) noexcept
        {
            switch (which)
            {
            case registration_block::kind::commit_hook:
                return commit;
            case registration_block::kind::rollback_hook:
                return rollback;
            case registration_block::kind::update_hook:
                return update;
            case registration_block::kind::progress_handler:
            case registration_block::kind::function:
            case registration_block::kind::collation:
                break;
            }
            return progress;
        }

        void hook_slots::replace(
            registration_block::kind which,
            registration_block* block
        ) noexcept
        {
            registration_block*& held = slot(which);
            registration_block* previous = held;
            held = block;
            if (previous != nullptr)
            {
                registration_block::released_by_sqlite(previous);
            }
        }
    } // detail
} // sqlite
} // sqlnx
