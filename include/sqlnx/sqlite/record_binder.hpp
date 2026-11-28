#ifndef SQLNX_SQLITE_RECORD_BINDER_HPP
#define SQLNX_SQLITE_RECORD_BINDER_HPP

/// \file
/// Binding a record's values to a statement's parameters one after the other, for record_traits::bind(), without an
/// error check after each. The mirror of sqlnx/sqlite/record_builder.hpp.
///
///     static sqlnx::sqlite::expected<void> bind(sqlnx::sqlite::statement& stmt, const point& p)
///     {
///         return sqlnx::sqlite::record_binder(stmt)
///             .bind(":x", p.x)           // by name, with its prefix (":x", "@x" or "$x")
///             .bind(2, p.y)              // or by index, from 1
///             .done();
///     }
///
/// Each bind() binds one value through value_traits. The first error wins: the binds after it don't happen, and
/// done() gives that error. done() also fails with primary_code::range when fewer values were bound than the
/// statement has parameters, so a parameter left out isn't silently NULL. The steps are for one chain: each takes
/// the binder as an rvalue.

#include "sqlnx/sqlite/config.hpp"
#include "sqlnx/sqlite/detail/out_of_memory.hpp"
#include "sqlnx/sqlite/error.hpp"
#include "sqlnx/sqlite/expected.hpp"
#include "sqlnx/sqlite/native/result.hpp"

#include <string>
#include <utility>

namespace sqlnx
{
namespace sqlite
{
    template <class Target>
    class basic_record_binder;

    /// Starts binding the parameters of `stmt` (a statement)
    template <class Target>
    basic_record_binder<Target> record_binder(Target& stmt) noexcept;

    /// The binds so far, or the first error. Made by record_binder().
    template <class Target>
    class basic_record_binder final
    {
    public:
        /// \name Binding
        ///@{
        template <class T>
        SQLNX_DETAIL_NODISCARD basic_record_binder bind(
            const char* name,
            const T& value
        ) &&
        {
            if (state)
            {
                state = stmt->bind_named(name, value);
                ++bound;
            }
            return std::move(*this);
        }

        template <class T>
        SQLNX_DETAIL_NODISCARD basic_record_binder bind(
            int index,
            const T& value
        ) &&
        {
            if (state)
            {
                state = stmt->bind_at(index, value);
                ++bound;
            }
            return std::move(*this);
        }
        ///@}

        /// The first error, or a primary_code::range error when a parameter got no value
        SQLNX_DETAIL_NODISCARD expected<void> done() && noexcept
        {
            if (!state)
            {
                return std::move(state);
            }
            const int parameters = stmt->native_statement().bind_parameter_count();
            if (bound < parameters)
            {
                const int missing = parameters - bound;
                return expected<void>(
                    unexpect,
                    detail::built_error(
                        native::result(static_cast<int>(native::primary_code::range)),
                        [missing, parameters]
                        {
                            return "the statement has " + std::to_string(parameters) + " parameters, and "
                                + std::to_string(missing) + " of them got no value";
                        }
                    )
                );
            }
            return {};
        }

    private:
        Target* stmt;
        expected<void> state;
        int bound = 0;

        template <class OtherTarget>
        friend basic_record_binder<OtherTarget> record_binder(OtherTarget& stmt) noexcept;

        explicit basic_record_binder(Target* target) noexcept
            : stmt(target)
        {
        }
    };

    template <class Target>
    basic_record_binder<Target> record_binder(Target& stmt) noexcept
    {
        return basic_record_binder<Target>(&stmt);
    }
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_RECORD_BINDER_HPP
