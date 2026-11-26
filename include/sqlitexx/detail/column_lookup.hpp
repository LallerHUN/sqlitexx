#ifndef SQLITEXX_DETAIL_COLUMN_LOOKUP_HPP
#define SQLITEXX_DETAIL_COLUMN_LOOKUP_HPP

/// \file
/// The column of each field of a record, looked up by name once and kept with the statement.

#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/expected.hpp"
#include "sqlitexx/native/statement_status.hpp"
#include "sqlitexx/statement.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace sqlitexx
{
namespace detail
{
    /// One address per type, to tell records apart without RTTI
    template <class T>
    struct type_key
    {
        static const char id;
    };

    template <class T>
    const char type_key<T>::id = 0;

    class column_lookup final
    {
    public:
        /// The column of each field in `list`, in order. Found by name on the first row, then kept with the statement
        /// until a schema change prepares it again or another record is read from it. nullptr when there is no memory
        /// to keep them: each field is then looked up by itself.
        template <
            class T,
            class List>
        static expected<const int*> find(
            const statement& row,
            const List& list
        ) noexcept
        {
            statement::record_columns& kept = row.field_columns;
            const int reprepares = const_cast<native::statement&>(row.stmt).status(native::statement_status::reprepare);
            if (kept.record == &type_key<T>::id && kept.reprepares == reprepares)
            {
                return expected<const int*>(kept.indexes.data());
            }
            kept.record = nullptr;
#if SQLITEXX_DETAIL_EXCEPTIONS
            try
            {
                kept.indexes.resize(std::tuple_size<List>::value);
            }
            catch (const std::bad_alloc&)
            {
                return expected<const int*>(static_cast<const int*>(nullptr));
            }
#else
            kept.indexes.resize(std::tuple_size<List>::value);
#endif
            expected<void> found =
                fill<0>(row, list, kept.indexes, std::integral_constant<bool, std::tuple_size<List>::value == 0>());
            if (!found)
            {
                return expected<const int*>(unexpect, std::move(found.error()));
            }
            kept.record = &type_key<T>::id;
            kept.reprepares = reprepares;
            return expected<const int*>(kept.indexes.data());
        }

    private:
        template <
            std::size_t I,
            class List>
        static expected<void> fill(
            const statement&,
            const List&,
            std::vector<int>&,
            std::true_type /*past the end*/
        ) noexcept
        {
            return {};
        }

        template <
            std::size_t I,
            class List>
        static expected<void> fill(
            const statement& row,
            const List& list,
            std::vector<int>& indexes,
            std::false_type /*past the end*/
        ) noexcept
        {
            expected<int> index = row.column_index(std::get<I>(list).name);
            if (!index)
            {
                return expected<void>(unexpect, std::move(index.error()));
            }
            indexes[I] = *index;
            return fill<I + 1>(
                row,
                list,
                indexes,
                std::integral_constant<bool, I + 1 == std::tuple_size<List>::value>()
            );
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_COLUMN_LOOKUP_HPP
