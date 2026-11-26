#ifndef SQLITEXX_DETAIL_STATEMENT_CACHE_HPP
#define SQLITEXX_DETAIL_STATEMENT_CACHE_HPP

/// \file
/// The statements a database keeps for query() and execute(sql, args...), keyed by their exact SQL text.

#include "sqlitexx/config.hpp"
#include "sqlitexx/statement.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace sqlitexx
{
namespace detail
{
    /// Idle prepared statements, the least recently used first. A statement is taken out while it is in use and given
    /// back afterwards, reset and with its bindings cleared; when the cache is full, the oldest one is finalized.
    class statement_cache final
    {
    public:
        explicit statement_cache(std::size_t statements) noexcept;

        statement_cache(const statement_cache&) = delete;
        statement_cache& operator=(const statement_cache&) = delete;

        /// \name Lending
        /// take() moves an idle statement prepared from exactly `sql` out of the cache, with its key; false when
        /// there is none. give_back() keeps it again, or finalizes it when the cache is off or out of memory.
        ///@{
        bool take(
            string_view sql,
            std::string& key,
            statement& taken
        ) noexcept;
        void give_back(
            std::string&& key,
            statement&& returned
        ) noexcept;
        ///@}

        /// \name Size
        ///@{
        void resize(std::size_t statements) noexcept;
        std::size_t capacity() const noexcept;
        ///@}

    private:
        struct entry
        {
            std::string sql;
            statement stmt;
        };

        /// Oldest first
        std::vector<entry> entries;
        std::size_t limit;
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_STATEMENT_CACHE_HPP
