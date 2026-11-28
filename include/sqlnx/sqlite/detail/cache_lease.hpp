#ifndef SQLNX_SQLITE_DETAIL_CACHE_LEASE_HPP
#define SQLNX_SQLITE_DETAIL_CACHE_LEASE_HPP

/// \file
/// A statement borrowed from a database's statement cache, and the promise to give it back.

#include "sqlnx/sqlite/detail/statement_cache.hpp"
#include "sqlnx/sqlite/expected.hpp"
#include "sqlnx/sqlite/statement.hpp"

#include <memory>
#include <string>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        /// Where a borrowed statement goes back to. It refers to the cache weakly: a statement that outlives its
        /// database is simply finalized.
        class cache_lease final
        {
        public:
            cache_lease() noexcept = default;

            cache_lease(
                std::weak_ptr<statement_cache> lender,
                std::string&& sql
            ) noexcept
                : owner(std::move(lender)),
                  key(std::move(sql))
            {
            }

            cache_lease(cache_lease&&) noexcept = default;
            cache_lease& operator=(cache_lease&&) noexcept = default;

            /// Gives `returned` back to its cache, if the cache still exists; the lease is spent afterwards. Without a
            /// cache, `returned` is left as it is, for its owner to finalize.
            void give_back(statement&& returned) noexcept
            {
                std::shared_ptr<statement_cache> cache = owner.lock();
                owner.reset();
                if (cache)
                {
                    cache->give_back(std::move(key), std::move(returned));
                }
            }

        private:
            std::weak_ptr<statement_cache> owner;
            std::string key;
        };

        /// What borrowing gives: the statement, or why there is none, and where it goes back to
        struct lent_statement
        {
            expected<statement> stmt;
            cache_lease lease;
        };
    } // detail
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_DETAIL_CACHE_LEASE_HPP
