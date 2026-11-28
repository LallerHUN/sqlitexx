#include "impl_sqlnx.hpp"

#include <utility>

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        statement_cache::statement_cache(std::size_t statements) noexcept
            : limit(statements)
        {
        }

        bool statement_cache::take(
            string_view sql,
            std::string& key,
            statement& taken
        ) noexcept
        {
            // The most recently used first: a loop running the same SQL finds it at once
            for (std::size_t i = entries.size(); i > 0; --i)
            {
                entry& candidate = entries[i - 1];
                if (string_view(candidate.sql) == sql)
                {
                    key = std::move(candidate.sql);
                    taken = std::move(candidate.stmt);
                    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i - 1));
                    return true;
                }
            }
            return false;
        }

        void statement_cache::give_back(
            std::string&& key,
            statement&& returned
        ) noexcept
        {
            if (limit == 0 || !returned.native_statement())
            {
                return;
            }
            // Nothing of the last run stays: reset() reports how it ended, which its caller already received, and the
            // values bound to it (a blob, say) are released
            // A burst of one query would otherwise fill the cache with copies of it and push every other statement out
            std::size_t copies = 0;
            for (const entry& kept : entries)
            {
                if (kept.sql == key)
                {
                    ++copies;
                }
            }
            if (copies >= copies_per_sql)
            {
                return;
            }
            native::statement& stmt = returned.native_statement();
            static_cast<void>(stmt.reset());
            static_cast<void>(stmt.clear_bindings());
            if (entries.size() >= limit)
            {
                entries.erase(entries.begin());
            }
#if SQLNX_DETAIL_EXCEPTIONS
            try
            {
                entries.push_back(entry{std::move(key), std::move(returned)});
            }
            catch (const std::bad_alloc&)
            {
                // Not kept: only a missed speed-up. `returned` is finalized by its owner.
            }
#else
            entries.push_back(entry{std::move(key), std::move(returned)});
#endif
        }

        void statement_cache::resize(std::size_t statements) noexcept
        {
            limit = statements;
            if (entries.size() > limit)
            {
                entries.erase(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(entries.size() - limit));
            }
        }

        std::size_t statement_cache::capacity() const noexcept
        {
            return limit;
        }
    } // detail
} // sqlite
} // sqlnx
