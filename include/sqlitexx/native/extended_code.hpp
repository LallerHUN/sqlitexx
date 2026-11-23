#ifndef SQLITEXX_NATIVE_EXTENDED_CODE_HPP
#define SQLITEXX_NATIVE_EXTENDED_CODE_HPP

namespace sqlitexx
{
namespace native
{
    // The extended result codes: a primary code in the low 8 bits and the detail above them. Values match
    // SQLITE_<PRIMARY>_<DETAIL> in sqlite3.h, so constraint_unique is SQLITE_CONSTRAINT_UNIQUE. A code a newer SQLite
    // adds is not listed here; compare result::code() with its number instead.
    enum class extended_code : int
    {
        // error
        error_missing_collseq = 1 | (1 << 8),
        error_retry = 1 | (2 << 8),
        error_snapshot = 1 | (3 << 8),
        error_reservesize = 1 | (4 << 8),
        error_key = 1 | (5 << 8),
        error_unable = 1 | (6 << 8),

        // ioerr
        ioerr_read = 10 | (1 << 8),
        ioerr_short_read = 10 | (2 << 8),
        ioerr_write = 10 | (3 << 8),
        ioerr_fsync = 10 | (4 << 8),
        ioerr_dir_fsync = 10 | (5 << 8),
        ioerr_truncate = 10 | (6 << 8),
        ioerr_fstat = 10 | (7 << 8),
        ioerr_unlock = 10 | (8 << 8),
        ioerr_rdlock = 10 | (9 << 8),
        ioerr_delete = 10 | (10 << 8),
        ioerr_blocked = 10 | (11 << 8),
        ioerr_nomem = 10 | (12 << 8),
        ioerr_access = 10 | (13 << 8),
        ioerr_checkreservedlock = 10 | (14 << 8),
        ioerr_lock = 10 | (15 << 8),
        ioerr_close = 10 | (16 << 8),
        ioerr_dir_close = 10 | (17 << 8),
        ioerr_shmopen = 10 | (18 << 8),
        ioerr_shmsize = 10 | (19 << 8),
        ioerr_shmlock = 10 | (20 << 8),
        ioerr_shmmap = 10 | (21 << 8),
        ioerr_seek = 10 | (22 << 8),
        ioerr_delete_noent = 10 | (23 << 8),
        ioerr_mmap = 10 | (24 << 8),
        ioerr_gettemppath = 10 | (25 << 8),
        ioerr_convpath = 10 | (26 << 8),
        ioerr_vnode = 10 | (27 << 8),
        ioerr_auth = 10 | (28 << 8),
        ioerr_begin_atomic = 10 | (29 << 8),
        ioerr_commit_atomic = 10 | (30 << 8),
        ioerr_rollback_atomic = 10 | (31 << 8),
        ioerr_data = 10 | (32 << 8),
        ioerr_corruptfs = 10 | (33 << 8),
        ioerr_in_page = 10 | (34 << 8),
        ioerr_badkey = 10 | (35 << 8),
        ioerr_codec = 10 | (36 << 8),

        // locked
        locked_sharedcache = 6 | (1 << 8),
        locked_vtab = 6 | (2 << 8),

        // busy
        busy_recovery = 5 | (1 << 8),
        busy_snapshot = 5 | (2 << 8),
        busy_timeout = 5 | (3 << 8),

        // cantopen
        cantopen_notempdir = 14 | (1 << 8),
        cantopen_isdir = 14 | (2 << 8),
        cantopen_fullpath = 14 | (3 << 8),
        cantopen_convpath = 14 | (4 << 8),
        cantopen_symlink = 14 | (6 << 8),

        // corrupt
        corrupt_vtab = 11 | (1 << 8),
        corrupt_sequence = 11 | (2 << 8),
        corrupt_index = 11 | (3 << 8),

        // readonly
        readonly_recovery = 8 | (1 << 8),
        readonly_cantlock = 8 | (2 << 8),
        readonly_rollback = 8 | (3 << 8),
        readonly_dbmoved = 8 | (4 << 8),
        readonly_cantinit = 8 | (5 << 8),
        readonly_directory = 8 | (6 << 8),

        // abort
        abort_rollback = 4 | (2 << 8),

        // constraint
        constraint_check = 19 | (1 << 8),
        constraint_commithook = 19 | (2 << 8),
        constraint_foreignkey = 19 | (3 << 8),
        constraint_function = 19 | (4 << 8),
        constraint_notnull = 19 | (5 << 8),
        constraint_primarykey = 19 | (6 << 8),
        constraint_trigger = 19 | (7 << 8),
        constraint_unique = 19 | (8 << 8),
        constraint_vtab = 19 | (9 << 8),
        constraint_rowid = 19 | (10 << 8),
        constraint_pinned = 19 | (11 << 8),
        constraint_datatype = 19 | (12 << 8),

        // notice
        notice_recover_wal = 27 | (1 << 8),
        notice_recover_rollback = 27 | (2 << 8),
        notice_rbu = 27 | (3 << 8),

        // warning
        warning_autoindex = 28 | (1 << 8),

        // auth
        auth_user = 23 | (1 << 8),

        // ok
        ok_load_permanently = 0 | (1 << 8),
    };

} // native
} // sqlitexx

#endif // SQLITEXX_NATIVE_EXTENDED_CODE_HPP
