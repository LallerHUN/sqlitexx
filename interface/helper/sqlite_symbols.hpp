#ifndef SQLITEXX_NATIVE_SQLITE_SYMBOLS_HPP
#define SQLITEXX_NATIVE_SQLITE_SYMBOLS_HPP

extern "C"
{

struct sqlite3;
struct sqlite3_stmt;

using sqlite3_open_ptr = int(const char* /*filename*/, sqlite3** /*ppDb*/);
using sqlite3_open_v2_ptr = int(const char* /*filename*/, int /*flags*/, sqlite3** /*ppDb*/);

using sqlite3_prepare_ptr = int(sqlite3* /*pDb*/, const char* /*zSql*/, int /*nByte*/, sqlite3_stmt** /*ppStmt*/, const char** /*pzTail*/);
using sqlite3_prepare_v2_ptr = int(sqlite3* /*pDb*/, const char* /*zSql*/, int /*nByte*/, sqlite3_stmt** /*ppStmt*/, const char** /*pzTail*/);
using sqlite3_prepare_v3_ptr = int(sqlite3* /*pDb*/, const char* /*zSql*/, int /*nByte*/, unsigned int /*prepFlags*/, sqlite3_stmt** /*ppStmt*/, const char** /*pzTail*/);

}

#endif // SQLITEXX_NATIVE_SQLITE_SYMBOLS_HPP
