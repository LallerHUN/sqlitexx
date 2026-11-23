#ifndef SQLITEXX_NATIVE_CONNECTION_HPP
#define SQLITEXX_NATIVE_CONNECTION_HPP

#include "sqlite_symbols.hpp"

namespace sqlitexx
{
namespace native
{
    struct connection final
    {
        int prepare(
                const char* sql
                , statement& stmt
                , const char** pzTail
        );

        int prepare_v2(
                const char* sql
                , statement& stmt
                , const char** pzTail
        );

        int prepare_v3(
                const char* sql
                , int flags
                , statement& stmt
                , const char** pzTail
        );

        int prepare(
                const wchar_t* sql
                , statement& stmt
                , const wchar_t** pzTail
        );

        int prepare_v2(
                const wchar_t* sql
                , statement& stmt
                , const wchar_t** pzTail
        );

        int prepare_v3(
                const wchar_t* sql
                , int flags
                , statement& stmt
                , const wchar_t** pzTail
        );

        int close();

        int close_v2();

        connection() = default;

        connection(
                sqlite3* db
        );

        ~connection();

    private:
        sqlite3* connection_hndl = nullptr;

        friend int open(const char*, connection&);
        friend int open(const wchar_t*, connection&);
        friend int open(const char*, int, const char*, connection&);
    };

    int open(
            const char* file_name
            , connection& connection
    );

    int open(
            const wchar_t* fileName
            , connection& connection
    );

    int open(
            const char* fileName
            , int flags
            , const char* zVfs
            , connection& connection
    );

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_CONNECTION_HPP
