#ifndef SQLNX_SQLITE_DETAIL_PARAMETER_ERRORS_HPP
#define SQLNX_SQLITE_DETAIL_PARAMETER_ERRORS_HPP

/// \file
/// The errors for binding a record whose fields don't match the statement's parameters.

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
