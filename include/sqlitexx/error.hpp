#ifndef SQLITEXX_ERROR_HPP
#define SQLITEXX_ERROR_HPP

#include "sqlitexx/native/result.hpp"

#include <string>
#include <utility>

namespace sqlitexx
{
// What went wrong: a SQLite result code (extended) and the message that came with it.
//
// Errors sqlitexx finds itself use SQLite codes too, so one kind of check covers both:
//  - primary_code::done      a query that had to produce a row produced none (statement::first)
//  - primary_code::mismatch  a value does not fit the C++ type: NULL read into a non-optional type, or an integer
//                            out of the type's range
//  - primary_code::misuse    the API was used wrongly, for example prepare() given no statement or several
class error final
{
public:
    error(
        native::result code,
        std::string message
    )
        : result_code(code),
          text(std::move(message))
    {
    }

    native::result code() const noexcept
    {
        return result_code;
    }

    native::primary_code primary() const noexcept
    {
        return result_code.primary();
    }

    const std::string& message() const noexcept
    {
        return text;
    }

private:
    native::result result_code;
    std::string text;
};

} // sqlitexx

#endif // SQLITEXX_ERROR_HPP
