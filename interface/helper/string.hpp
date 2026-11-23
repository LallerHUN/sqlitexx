#ifndef SQLITEXX_NATIVE_STRING_HPP
#define SQLITEXX_NATIVE_STRING_HPP

#include "helper/error.hpp"
#include "helper/sqlite_symbols.hpp"

namespace sqlitexx
{
namespace helper
{
    struct string_v_table
    {
        // open
        virtual int open(
            const char* filename
            , sqlite3** ppDb
        ) = 0;

        virtual int open_v2(
            const char* filename
            , int flags
            , sqlite3** ppDb
        ) = 0;

        // prepare
        virtual int prepare(
            sqlite3* pDb
            , const char* zSql
            , int nByte
            , sqlite3_stmt** ppStmt
            , const char** pzTail
        ) = 0;

        virtual int prepare_v2(
            sqlite3* pDb
            , const char* zSql
            , int nByte
            , sqlite3_stmt** ppStmt
            , const char** pzTail
        ) = 0;

        virtual int prepare_v3(
            sqlite3* pDb
            , const char* zSql
            , int nByte
            , sqlite3_stmt** ppStmt
            , const char** pzTail
        ) = 0;
    };

    struct utf8_v_table : public string_v_table
    {
        // open
        int open(
            const char* filename
            , sqlite3** ppDb
        ) override;

        int open_v2(
            const char* filename
            , int flags
            , sqlite3** ppDb
        ) override;

        // prepare
        virtual int prepare(
            sqlite3* pDb
            , const char* zSql
            , int nByte
            , sqlite3_stmt** ppStmt
            , const char** pzTail
        ) override;

        virtual int prepare_v2(
            sqlite3* pDb
            , const char* zSql
            , int nByte
            , sqlite3_stmt** ppStmt
            , const char** pzTail
        ) override;

        virtual int prepare_v3(
            sqlite3* pDb
            , const char* zSql
            , int nByte
            , sqlite3_stmt** ppStmt
            , const char** pzTail
        ) override;
    };
    
} // helper
} // sqlitexx

#endif // SQLITEXX_NATIVE_STRING_HPP
