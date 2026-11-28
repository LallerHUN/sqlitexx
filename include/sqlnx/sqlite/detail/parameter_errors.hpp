#ifndef SQLNX_SQLITE_DETAIL_PARAMETER_ERRORS_HPP
#define SQLNX_SQLITE_DETAIL_PARAMETER_ERRORS_HPP

/// \file
/// The errors for binding values that don't match the statement's parameters.

#include "sqlnx/sqlite/detail/out_of_memory.hpp"
#include "sqlnx/sqlite/error.hpp"
#include "sqlnx/sqlite/native/result.hpp"

#include <string>

namespace sqlnx
{
namespace sqlite
{
    namespace detail
    {
        /// Binding by position: one argument per parameter, so none is left NULL by accident
        inline error argument_count(
            int arguments,
            int parameters
        ) noexcept
        {
            return built_error(
                native::result(static_cast<int>(native::primary_code::range)),
                [arguments, parameters]
                {
                    return std::to_string(arguments) + (arguments == 1 ? " argument" : " arguments") + " for "
                        + std::to_string(parameters) + (parameters == 1 ? " parameter" : " parameters")
                        + ": give one value per parameter, or bind some with bind_at() or bind_named()";
                }
            );
        }

        inline error positional_parameter(int index) noexcept
        {
            return built_error(
                native::result(static_cast<int>(native::primary_code::range)),
                [index]
                {
                    return "parameter " + std::to_string(index)
                        + " has no name: a record binds named parameters (:name, @name or $name)";
                }
            );
        }

        inline error parameter_without_field(const char* name) noexcept
        {
            return built_error(
                native::result(static_cast<int>(native::primary_code::range)),
                [name]
                {
                    return std::string("the record has no field for parameter ") + name;
                }
            );
        }
    } // detail
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_DETAIL_PARAMETER_ERRORS_HPP
