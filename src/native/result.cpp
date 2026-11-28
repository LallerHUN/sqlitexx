#include "impl_sqlnx.hpp"

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        // The public headers do not include sqlite3.h, so they repeat its constants. Keep them honest.
        // clang-format off: these are assertions, not the function declarations clang-format takes them for
    static_assert(static_cast<int>(primary_code::ok) == SQLITE_OK, "");
    static_assert(static_cast<int>(primary_code::error) == SQLITE_ERROR, "");
    static_assert(static_cast<int>(primary_code::busy) == SQLITE_BUSY, "");
    static_assert(static_cast<int>(primary_code::locked) == SQLITE_LOCKED, "");
    static_assert(static_cast<int>(primary_code::nomem) == SQLITE_NOMEM, "");
    static_assert(static_cast<int>(primary_code::ioerr) == SQLITE_IOERR, "");
    static_assert(static_cast<int>(primary_code::cantopen) == SQLITE_CANTOPEN, "");
    static_assert(static_cast<int>(primary_code::toobig) == SQLITE_TOOBIG, "");
    static_assert(static_cast<int>(primary_code::constraint) == SQLITE_CONSTRAINT, "");
    static_assert(static_cast<int>(primary_code::misuse) == SQLITE_MISUSE, "");
    static_assert(static_cast<int>(primary_code::range) == SQLITE_RANGE, "");
    static_assert(static_cast<int>(primary_code::notadb) == SQLITE_NOTADB, "");
    static_assert(static_cast<int>(primary_code::warning) == SQLITE_WARNING, "");
    static_assert(static_cast<int>(primary_code::row) == SQLITE_ROW, "");
    static_assert(static_cast<int>(primary_code::done) == SQLITE_DONE, "");

    // Extended codes, where this SQLite defines them (an older system SQLite lacks the newest)
#if defined(SQLITE_ERROR_MISSING_COLLSEQ)
    static_assert(static_cast<int>(extended_code::error_missing_collseq) == SQLITE_ERROR_MISSING_COLLSEQ, "");
#endif
#if defined(SQLITE_ERROR_RETRY)
    static_assert(static_cast<int>(extended_code::error_retry) == SQLITE_ERROR_RETRY, "");
#endif
#if defined(SQLITE_ERROR_SNAPSHOT)
    static_assert(static_cast<int>(extended_code::error_snapshot) == SQLITE_ERROR_SNAPSHOT, "");
#endif
#if defined(SQLITE_ERROR_RESERVESIZE)
    static_assert(static_cast<int>(extended_code::error_reservesize) == SQLITE_ERROR_RESERVESIZE, "");
#endif
#if defined(SQLITE_ERROR_KEY)
    static_assert(static_cast<int>(extended_code::error_key) == SQLITE_ERROR_KEY, "");
#endif
#if defined(SQLITE_ERROR_UNABLE)
    static_assert(static_cast<int>(extended_code::error_unable) == SQLITE_ERROR_UNABLE, "");
#endif
#if defined(SQLITE_IOERR_READ)
    static_assert(static_cast<int>(extended_code::ioerr_read) == SQLITE_IOERR_READ, "");
#endif
#if defined(SQLITE_IOERR_SHORT_READ)
    static_assert(static_cast<int>(extended_code::ioerr_short_read) == SQLITE_IOERR_SHORT_READ, "");
#endif
#if defined(SQLITE_IOERR_WRITE)
    static_assert(static_cast<int>(extended_code::ioerr_write) == SQLITE_IOERR_WRITE, "");
#endif
#if defined(SQLITE_IOERR_FSYNC)
    static_assert(static_cast<int>(extended_code::ioerr_fsync) == SQLITE_IOERR_FSYNC, "");
#endif
#if defined(SQLITE_IOERR_DIR_FSYNC)
    static_assert(static_cast<int>(extended_code::ioerr_dir_fsync) == SQLITE_IOERR_DIR_FSYNC, "");
#endif
#if defined(SQLITE_IOERR_TRUNCATE)
    static_assert(static_cast<int>(extended_code::ioerr_truncate) == SQLITE_IOERR_TRUNCATE, "");
#endif
#if defined(SQLITE_IOERR_FSTAT)
    static_assert(static_cast<int>(extended_code::ioerr_fstat) == SQLITE_IOERR_FSTAT, "");
#endif
#if defined(SQLITE_IOERR_UNLOCK)
    static_assert(static_cast<int>(extended_code::ioerr_unlock) == SQLITE_IOERR_UNLOCK, "");
#endif
#if defined(SQLITE_IOERR_RDLOCK)
    static_assert(static_cast<int>(extended_code::ioerr_rdlock) == SQLITE_IOERR_RDLOCK, "");
#endif
#if defined(SQLITE_IOERR_DELETE)
    static_assert(static_cast<int>(extended_code::ioerr_delete) == SQLITE_IOERR_DELETE, "");
#endif
#if defined(SQLITE_IOERR_BLOCKED)
    static_assert(static_cast<int>(extended_code::ioerr_blocked) == SQLITE_IOERR_BLOCKED, "");
#endif
#if defined(SQLITE_IOERR_NOMEM)
    static_assert(static_cast<int>(extended_code::ioerr_nomem) == SQLITE_IOERR_NOMEM, "");
#endif
#if defined(SQLITE_IOERR_ACCESS)
    static_assert(static_cast<int>(extended_code::ioerr_access) == SQLITE_IOERR_ACCESS, "");
#endif
#if defined(SQLITE_IOERR_CHECKRESERVEDLOCK)
    static_assert(static_cast<int>(extended_code::ioerr_checkreservedlock) == SQLITE_IOERR_CHECKRESERVEDLOCK, "");
#endif
#if defined(SQLITE_IOERR_LOCK)
    static_assert(static_cast<int>(extended_code::ioerr_lock) == SQLITE_IOERR_LOCK, "");
#endif
#if defined(SQLITE_IOERR_CLOSE)
    static_assert(static_cast<int>(extended_code::ioerr_close) == SQLITE_IOERR_CLOSE, "");
#endif
#if defined(SQLITE_IOERR_DIR_CLOSE)
    static_assert(static_cast<int>(extended_code::ioerr_dir_close) == SQLITE_IOERR_DIR_CLOSE, "");
#endif
#if defined(SQLITE_IOERR_SHMOPEN)
    static_assert(static_cast<int>(extended_code::ioerr_shmopen) == SQLITE_IOERR_SHMOPEN, "");
#endif
#if defined(SQLITE_IOERR_SHMSIZE)
    static_assert(static_cast<int>(extended_code::ioerr_shmsize) == SQLITE_IOERR_SHMSIZE, "");
#endif
#if defined(SQLITE_IOERR_SHMLOCK)
    static_assert(static_cast<int>(extended_code::ioerr_shmlock) == SQLITE_IOERR_SHMLOCK, "");
#endif
#if defined(SQLITE_IOERR_SHMMAP)
    static_assert(static_cast<int>(extended_code::ioerr_shmmap) == SQLITE_IOERR_SHMMAP, "");
#endif
#if defined(SQLITE_IOERR_SEEK)
    static_assert(static_cast<int>(extended_code::ioerr_seek) == SQLITE_IOERR_SEEK, "");
#endif
#if defined(SQLITE_IOERR_DELETE_NOENT)
    static_assert(static_cast<int>(extended_code::ioerr_delete_noent) == SQLITE_IOERR_DELETE_NOENT, "");
#endif
#if defined(SQLITE_IOERR_MMAP)
    static_assert(static_cast<int>(extended_code::ioerr_mmap) == SQLITE_IOERR_MMAP, "");
#endif
#if defined(SQLITE_IOERR_GETTEMPPATH)
    static_assert(static_cast<int>(extended_code::ioerr_gettemppath) == SQLITE_IOERR_GETTEMPPATH, "");
#endif
#if defined(SQLITE_IOERR_CONVPATH)
    static_assert(static_cast<int>(extended_code::ioerr_convpath) == SQLITE_IOERR_CONVPATH, "");
#endif
#if defined(SQLITE_IOERR_VNODE)
    static_assert(static_cast<int>(extended_code::ioerr_vnode) == SQLITE_IOERR_VNODE, "");
#endif
#if defined(SQLITE_IOERR_AUTH)
    static_assert(static_cast<int>(extended_code::ioerr_auth) == SQLITE_IOERR_AUTH, "");
#endif
#if defined(SQLITE_IOERR_BEGIN_ATOMIC)
    static_assert(static_cast<int>(extended_code::ioerr_begin_atomic) == SQLITE_IOERR_BEGIN_ATOMIC, "");
#endif
#if defined(SQLITE_IOERR_COMMIT_ATOMIC)
    static_assert(static_cast<int>(extended_code::ioerr_commit_atomic) == SQLITE_IOERR_COMMIT_ATOMIC, "");
#endif
#if defined(SQLITE_IOERR_ROLLBACK_ATOMIC)
    static_assert(static_cast<int>(extended_code::ioerr_rollback_atomic) == SQLITE_IOERR_ROLLBACK_ATOMIC, "");
#endif
#if defined(SQLITE_IOERR_DATA)
    static_assert(static_cast<int>(extended_code::ioerr_data) == SQLITE_IOERR_DATA, "");
#endif
#if defined(SQLITE_IOERR_CORRUPTFS)
    static_assert(static_cast<int>(extended_code::ioerr_corruptfs) == SQLITE_IOERR_CORRUPTFS, "");
#endif
#if defined(SQLITE_IOERR_IN_PAGE)
    static_assert(static_cast<int>(extended_code::ioerr_in_page) == SQLITE_IOERR_IN_PAGE, "");
#endif
#if defined(SQLITE_IOERR_BADKEY)
    static_assert(static_cast<int>(extended_code::ioerr_badkey) == SQLITE_IOERR_BADKEY, "");
#endif
#if defined(SQLITE_IOERR_CODEC)
    static_assert(static_cast<int>(extended_code::ioerr_codec) == SQLITE_IOERR_CODEC, "");
#endif
#if defined(SQLITE_LOCKED_SHAREDCACHE)
    static_assert(static_cast<int>(extended_code::locked_sharedcache) == SQLITE_LOCKED_SHAREDCACHE, "");
#endif
#if defined(SQLITE_LOCKED_VTAB)
    static_assert(static_cast<int>(extended_code::locked_vtab) == SQLITE_LOCKED_VTAB, "");
#endif
#if defined(SQLITE_BUSY_RECOVERY)
    static_assert(static_cast<int>(extended_code::busy_recovery) == SQLITE_BUSY_RECOVERY, "");
#endif
#if defined(SQLITE_BUSY_SNAPSHOT)
    static_assert(static_cast<int>(extended_code::busy_snapshot) == SQLITE_BUSY_SNAPSHOT, "");
#endif
#if defined(SQLITE_BUSY_TIMEOUT)
    static_assert(static_cast<int>(extended_code::busy_timeout) == SQLITE_BUSY_TIMEOUT, "");
#endif
#if defined(SQLITE_CANTOPEN_NOTEMPDIR)
    static_assert(static_cast<int>(extended_code::cantopen_notempdir) == SQLITE_CANTOPEN_NOTEMPDIR, "");
#endif
#if defined(SQLITE_CANTOPEN_ISDIR)
    static_assert(static_cast<int>(extended_code::cantopen_isdir) == SQLITE_CANTOPEN_ISDIR, "");
#endif
#if defined(SQLITE_CANTOPEN_FULLPATH)
    static_assert(static_cast<int>(extended_code::cantopen_fullpath) == SQLITE_CANTOPEN_FULLPATH, "");
#endif
#if defined(SQLITE_CANTOPEN_CONVPATH)
    static_assert(static_cast<int>(extended_code::cantopen_convpath) == SQLITE_CANTOPEN_CONVPATH, "");
#endif
#if defined(SQLITE_CANTOPEN_SYMLINK)
    static_assert(static_cast<int>(extended_code::cantopen_symlink) == SQLITE_CANTOPEN_SYMLINK, "");
#endif
#if defined(SQLITE_CORRUPT_VTAB)
    static_assert(static_cast<int>(extended_code::corrupt_vtab) == SQLITE_CORRUPT_VTAB, "");
#endif
#if defined(SQLITE_CORRUPT_SEQUENCE)
    static_assert(static_cast<int>(extended_code::corrupt_sequence) == SQLITE_CORRUPT_SEQUENCE, "");
#endif
#if defined(SQLITE_CORRUPT_INDEX)
    static_assert(static_cast<int>(extended_code::corrupt_index) == SQLITE_CORRUPT_INDEX, "");
#endif
#if defined(SQLITE_READONLY_RECOVERY)
    static_assert(static_cast<int>(extended_code::readonly_recovery) == SQLITE_READONLY_RECOVERY, "");
#endif
#if defined(SQLITE_READONLY_CANTLOCK)
    static_assert(static_cast<int>(extended_code::readonly_cantlock) == SQLITE_READONLY_CANTLOCK, "");
#endif
#if defined(SQLITE_READONLY_ROLLBACK)
    static_assert(static_cast<int>(extended_code::readonly_rollback) == SQLITE_READONLY_ROLLBACK, "");
#endif
#if defined(SQLITE_READONLY_DBMOVED)
    static_assert(static_cast<int>(extended_code::readonly_dbmoved) == SQLITE_READONLY_DBMOVED, "");
#endif
#if defined(SQLITE_READONLY_CANTINIT)
    static_assert(static_cast<int>(extended_code::readonly_cantinit) == SQLITE_READONLY_CANTINIT, "");
#endif
#if defined(SQLITE_READONLY_DIRECTORY)
    static_assert(static_cast<int>(extended_code::readonly_directory) == SQLITE_READONLY_DIRECTORY, "");
#endif
#if defined(SQLITE_ABORT_ROLLBACK)
    static_assert(static_cast<int>(extended_code::abort_rollback) == SQLITE_ABORT_ROLLBACK, "");
#endif
#if defined(SQLITE_CONSTRAINT_CHECK)
    static_assert(static_cast<int>(extended_code::constraint_check) == SQLITE_CONSTRAINT_CHECK, "");
#endif
#if defined(SQLITE_CONSTRAINT_COMMITHOOK)
    static_assert(static_cast<int>(extended_code::constraint_commithook) == SQLITE_CONSTRAINT_COMMITHOOK, "");
#endif
#if defined(SQLITE_CONSTRAINT_FOREIGNKEY)
    static_assert(static_cast<int>(extended_code::constraint_foreignkey) == SQLITE_CONSTRAINT_FOREIGNKEY, "");
#endif
#if defined(SQLITE_CONSTRAINT_FUNCTION)
    static_assert(static_cast<int>(extended_code::constraint_function) == SQLITE_CONSTRAINT_FUNCTION, "");
#endif
#if defined(SQLITE_CONSTRAINT_NOTNULL)
    static_assert(static_cast<int>(extended_code::constraint_notnull) == SQLITE_CONSTRAINT_NOTNULL, "");
#endif
#if defined(SQLITE_CONSTRAINT_PRIMARYKEY)
    static_assert(static_cast<int>(extended_code::constraint_primarykey) == SQLITE_CONSTRAINT_PRIMARYKEY, "");
#endif
#if defined(SQLITE_CONSTRAINT_TRIGGER)
    static_assert(static_cast<int>(extended_code::constraint_trigger) == SQLITE_CONSTRAINT_TRIGGER, "");
#endif
#if defined(SQLITE_CONSTRAINT_UNIQUE)
    static_assert(static_cast<int>(extended_code::constraint_unique) == SQLITE_CONSTRAINT_UNIQUE, "");
#endif
#if defined(SQLITE_CONSTRAINT_VTAB)
    static_assert(static_cast<int>(extended_code::constraint_vtab) == SQLITE_CONSTRAINT_VTAB, "");
#endif
#if defined(SQLITE_CONSTRAINT_ROWID)
    static_assert(static_cast<int>(extended_code::constraint_rowid) == SQLITE_CONSTRAINT_ROWID, "");
#endif
#if defined(SQLITE_CONSTRAINT_PINNED)
    static_assert(static_cast<int>(extended_code::constraint_pinned) == SQLITE_CONSTRAINT_PINNED, "");
#endif
#if defined(SQLITE_CONSTRAINT_DATATYPE)
    static_assert(static_cast<int>(extended_code::constraint_datatype) == SQLITE_CONSTRAINT_DATATYPE, "");
#endif
#if defined(SQLITE_NOTICE_RECOVER_WAL)
    static_assert(static_cast<int>(extended_code::notice_recover_wal) == SQLITE_NOTICE_RECOVER_WAL, "");
#endif
#if defined(SQLITE_NOTICE_RECOVER_ROLLBACK)
    static_assert(static_cast<int>(extended_code::notice_recover_rollback) == SQLITE_NOTICE_RECOVER_ROLLBACK, "");
#endif
#if defined(SQLITE_NOTICE_RBU)
    static_assert(static_cast<int>(extended_code::notice_rbu) == SQLITE_NOTICE_RBU, "");
#endif
#if defined(SQLITE_WARNING_AUTOINDEX)
    static_assert(static_cast<int>(extended_code::warning_autoindex) == SQLITE_WARNING_AUTOINDEX, "");
#endif
#if defined(SQLITE_AUTH_USER)
    static_assert(static_cast<int>(extended_code::auth_user) == SQLITE_AUTH_USER, "");
#endif
#if defined(SQLITE_OK_LOAD_PERMANENTLY)
    static_assert(static_cast<int>(extended_code::ok_load_permanently) == SQLITE_OK_LOAD_PERMANENTLY, "");
#endif

    static_assert(static_cast<int>(open_flags::readonly) == SQLITE_OPEN_READONLY, "");
    static_assert(static_cast<int>(open_flags::readwrite) == SQLITE_OPEN_READWRITE, "");
    static_assert(static_cast<int>(open_flags::create) == SQLITE_OPEN_CREATE, "");
    static_assert(static_cast<int>(open_flags::uri) == SQLITE_OPEN_URI, "");
    static_assert(static_cast<int>(open_flags::memory) == SQLITE_OPEN_MEMORY, "");
    static_assert(static_cast<int>(open_flags::nomutex) == SQLITE_OPEN_NOMUTEX, "");
    static_assert(static_cast<int>(open_flags::fullmutex) == SQLITE_OPEN_FULLMUTEX, "");
    static_assert(static_cast<int>(open_flags::sharedcache) == SQLITE_OPEN_SHAREDCACHE, "");
    static_assert(static_cast<int>(open_flags::privatecache) == SQLITE_OPEN_PRIVATECACHE, "");
    static_assert(static_cast<int>(open_flags::nofollow) == SQLITE_OPEN_NOFOLLOW, "");

    static_assert(static_cast<unsigned int>(prepare_flags::persistent) == SQLITE_PREPARE_PERSISTENT, "");
    static_assert(static_cast<unsigned int>(prepare_flags::normalize) == SQLITE_PREPARE_NORMALIZE, "");
    static_assert(static_cast<unsigned int>(prepare_flags::no_vtab) == SQLITE_PREPARE_NO_VTAB, "");

    static_assert(static_cast<int>(function_flags::deterministic) == SQLITE_DETERMINISTIC, "");
    static_assert(static_cast<int>(function_flags::directonly) == SQLITE_DIRECTONLY, "");
    static_assert(static_cast<int>(function_flags::innocuous) == SQLITE_INNOCUOUS, "");

    static_assert(static_cast<int>(value_type::integer) == SQLITE_INTEGER, "");
    static_assert(static_cast<int>(value_type::floating) == SQLITE_FLOAT, "");
    static_assert(static_cast<int>(value_type::text) == SQLITE_TEXT, "");
    static_assert(static_cast<int>(value_type::blob) == SQLITE_BLOB, "");
    static_assert(static_cast<int>(value_type::null) == SQLITE_NULL, "");
        // clang-format on

        const char* result::errstr() const noexcept
        {
            return sqlite3_errstr(value);
        }

    } // native
} // sqlite
} // sqlnx
