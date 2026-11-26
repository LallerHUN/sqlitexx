#ifndef SQLITEXX_DETAIL_PARAMETER_ERRORS_HPP
#define SQLITEXX_DETAIL_PARAMETER_ERRORS_HPP

/// \file
/// The errors for binding a record whose fields don't match the statement's parameters.

#include "sqlitexx/detail/out_of_memory.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/native/result.hpp"

#include <string>

namespace sqlitexx
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
} // sqlitexx

#endif // SQLITEXX_DETAIL_PARAMETER_ERRORS_HPP
