#ifndef SQLNX_SQLITE_NATIVE_HOOK_CALLBACKS_HPP
#define SQLNX_SQLITE_NATIVE_HOOK_CALLBACKS_HPP

namespace sqlnx
{
namespace sqlite
{
    namespace native
    {
        /// What changed a row, for update hooks. Values match SQLITE_INSERT, SQLITE_UPDATE and SQLITE_DELETE.
        enum class update_kind : int
        {
            remove = 9,
            insert = 18,
            update = 23,
        };

        /// The callbacks connection::commit_hook, rollback_hook, update_hook and progress_handler take, as in
        /// sqlite3_commit_hook, sqlite3_rollback_hook, sqlite3_update_hook and sqlite3_progress_handler:
        ///  - commit: nonzero turns the commit into a rollback
        ///  - update: the kind of change (an update_kind value), the schema, the table and the row's rowid
        ///  - progress: nonzero interrupts the running statement
        using commit_callback = int (*)(void* user_data);
        using rollback_callback = void (*)(void* user_data);
        using update_callback = void (*)(
            void* user_data,
            int kind,
            const char* schema,
            const char* table,
            long long rowid
        );
        using progress_callback = int (*)(void* user_data);

    } // native
} // sqlite
} // sqlnx

#endif // SQLNX_SQLITE_NATIVE_HOOK_CALLBACKS_HPP
