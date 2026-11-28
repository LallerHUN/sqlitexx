#include "impl_sqlnx.hpp"

#include <cstring>
#include <memory>
#include <new>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        registration_block::registration_block(
            sqlite3* connection,
            std::unique_ptr<char[]> registered_name,
            int arguments,
            kind registered,
            hook_slots* slots
        ) noexcept
            : handle(connection),
              name(std::move(registered_name)),
              argument_count(arguments),
              what(registered),
              hooks(slots)
        {
        }

        registration_block::~registration_block() = default;

        void registration_block::released_by_sqlite(void* block) noexcept
        {
            registration_block* released = static_cast<registration_block*>(block);
            // Nothing calls it any more: it ends now, as a callable registered without a token does
            released->end_callable();
            released->held_by_sqlite = false;
            if (!released->held_by_token)
            {
                delete released;
            }
        }

        std::unique_ptr<char[]> registration_block::copy_name(cstring_view name) noexcept
        {
            std::unique_ptr<char[]> copied(new (std::nothrow) char[name.size() + 1]);
            if (copied)
            {
                std::memcpy(copied.get(), name.c_str(), name.size() + 1);
            }
            return copied;
        }

        native::result registration_block::remove() noexcept
        {
            if (!held_by_sqlite)
            {
                return native::result(SQLITE_OK);
            }
            // A hook is cleared through the database's slots, which release the block. Clearing never fails.
            if (hooks != nullptr)
            {
                hooks->clear(what, this);
                return native::result(SQLITE_OK);
            }
            // Without callbacks, SQLite removes what is registered under the name (and count), and releases the block
            if (what == kind::function)
            {
                return native::result(sqlite3_create_function_v2(
                    handle,
                    name.get(),
                    argument_count,
                    SQLITE_UTF8,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                ));
            }
            return native::result(
                sqlite3_create_collation_v2(handle, name.get(), SQLITE_UTF8, nullptr, nullptr, nullptr)
            );
        }

        void registration_block::released_by_token() noexcept
        {
            held_by_token = false;
            next = nullptr;
            if (!held_by_sqlite)
            {
                delete this;
            }
        }
    } // detail

    registrations::registrations(registrations&& other) noexcept
        : latest(other.latest)
    {
        other.latest = nullptr;
    }

    registrations& registrations::operator=(registrations&& other) noexcept
    {
        if (this != &other)
        {
            end();
            latest = other.latest;
            other.latest = nullptr;
        }
        return *this;
    }

    registrations::~registrations()
    {
        end();
    }

    expected<void> registrations::remove() noexcept
    {
        expected<void> outcome;
        detail::registration_block** link = &latest;
        while (*link != nullptr)
        {
            detail::registration_block* block = *link;
            const native::result code = block->remove();
            if (code.failed())
            {
                if (outcome)
                {
                    outcome = expected<void>(unexpect, detail::make_error(block->handle, code));
                }
                link = &block->next;
                continue;
            }
            *link = block->next;
            block->released_by_token();
        }
        return outcome;
    }

    void registrations::release() noexcept
    {
        while (latest != nullptr)
        {
            detail::registration_block* block = latest;
            latest = block->next;
            block->released_by_token();
        }
    }

    bool registrations::empty() const noexcept
    {
        return latest == nullptr;
    }

    void registrations::adopt(detail::registration_block* block) noexcept
    {
        block->held_by_token = true;
        block->next = latest;
        latest = block;
    }

    void registrations::end() noexcept
    {
        static_cast<void>(remove());
        while (latest != nullptr)
        {
            detail::registration_block* block = latest;
            latest = block->next;
            block->end_callable();
            block->released_by_token();
        }
    }

} // sqlite
} // sqlnx
