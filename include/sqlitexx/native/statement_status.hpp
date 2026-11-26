#ifndef SQLITEXX_NATIVE_STATEMENT_STATUS_HPP
#define SQLITEXX_NATIVE_STATEMENT_STATUS_HPP

/// \file
/// The counters of statement::status() (sqlite3_stmt_status). Values match SQLITE_STMTSTATUS_* in sqlite3.h. The
/// filter counters need SQLite 3.38: before that, status() gives 0 for them without asking SQLite. An empty statement
/// gives 0 for every counter.

namespace sqlitexx
{
namespace native
{
    enum class statement_status : int
    {
        fullscan_step = 1, ///< Steps forward in a full table scan
        sort = 2, ///< Sort operations
        autoindex = 3, ///< Rows inserted into automatic indexes
        vm_step = 4, ///< Virtual machine operations run
        reprepare = 5, ///< Times the statement was prepared again, after a schema change for example
        run = 6, ///< Times the statement has run
        filter_miss = 7, ///< Bloom filter checks that found the row missing
        filter_hit = 8, ///< Bloom filter checks that passed
        memused = 99 ///< Bytes of heap the statement uses (never reset)
    };
} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_STATEMENT_STATUS_HPP
