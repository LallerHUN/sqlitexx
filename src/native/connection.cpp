#include "impl_native.hpp"

namespace sqlitexx
{
namespace native
{

    int connection::prepare(
            const char* sql
            , statement& stmt
            , const char** pzTail
    ) {
        return 
    }

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

    connection::connection(
            const char* fileName
    ) {
        
    }

        connection(
            const wchar_t* fileName
        );

        connection(
            const char* fileName
            , int flags
            , const char* zVfs
        );

        ~connection();

        

    int open(
            const char* file_name
            , connection& connection
    ) {
        connection.close_v2();
        return sqlite3_open(file_name, connection.connection_hndl);
    }

    int open(
            const wchar_t* file_name
            , connection& connection
    ) {
        connection.close_v2();
        return sqlite3_open16(reinterpret_cast<char*>(file_name), connection.connection_hndl);
    }

    int open(
            const char* file_name
            , int flags
            , const char* zVfs
            , connection& connection
    ) {
        connection.close_v2();
        return sqlite3_open_v2(file_name, connection.connection_hndl);
    }

} // native
} // sqlitexx