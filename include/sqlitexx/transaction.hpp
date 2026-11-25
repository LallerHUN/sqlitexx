#ifndef SQLITEXX_TRANSACTION_HPP
#define SQLITEXX_TRANSACTION_HPP

#include "sqlitexx/database.hpp"
#include "sqlitexx/error.hpp"
#include "sqlitexx/expected.hpp"

#include <utility>

extern "C"
{
    struct sqlite3;
}

namespace sqlitexx
{
// How BEGIN takes its locks (see https://sqlite.org/lang_transaction.html).
enum class transaction_mode
{
    // Locks are taken by the first read or write
    deferred,
    // The write lock is taken at once, so the transaction cannot fail later with SQLITE_BUSY on its first write
    immediate,
    // Like immediate; in WAL mode the same, otherwise readers are kept out too
    exclusive,
};

// A transaction that rolls back unless it is committed. Move-only.
//
//     expected<transaction> tx = transaction::begin(db);
//     ... statements ...
//     expected<void> committed = tx->commit();   // leaving the scope without commit() rolls back
//
// It must not outlive its database. When commit() fails, the transaction can still be open (a deferred constraint
// failed, or SQLITE_BUSY) or already rolled back by SQLite (for example after SQLITE_FULL); active() tells which, and
// an open transaction can be retried or rolled back.
class transaction final
{
public:
    transaction() noexcept = default;

    transaction(transaction&& other) noexcept;
    transaction& operator=(transaction&& other) noexcept;

    transaction(const transaction&) = delete;
    transaction& operator=(const transaction&) = delete;

    ~transaction();

    // Lifetime
    static expected<transaction> begin(
        database& target,
        transaction_mode mode = transaction_mode::deferred
    ) noexcept;

    expected<void> commit() noexcept;
    expected<void> rollback() noexcept;

    // True until commit() or rollback() ends the transaction, or SQLite rolls it back itself
    bool active() const noexcept;

private:
    sqlite3* db = nullptr;

    explicit transaction(sqlite3* handle) noexcept;

    expected<void> finish(const char* sql) noexcept;
    void rollback_if_open() noexcept;
};

// Runs `body` in a transaction: commits when it returns a value, rolls back when it returns an error. Gives back the
// body's result, or the error of BEGIN or COMMIT. `body` takes no arguments and returns an expected<T>.
//
//     expected<std::int64_t> id = sqlitexx::in_transaction(db, [&] {
//         return db.prepare("INSERT INTO user(name) VALUES(?)")
//             .and_then(sqlitexx::execute_with("Alice"))
//             .transform([&] { return db.last_insert_rowid(); });
//     });
template <class F>
auto in_transaction(
    database& db,
    F&& body,
    transaction_mode mode = transaction_mode::deferred
) -> decltype(std::forward<F>(body)())
{
    using result_type = decltype(std::forward<F>(body)());

    expected<transaction> tx = transaction::begin(db, mode);
    if (!tx)
    {
        return result_type(unexpect, std::move(tx.error()));
    }

    result_type result = std::forward<F>(body)();
    if (!result)
    {
        return result; // tx rolls back as it goes out of scope
    }

    expected<void> committed = tx->commit();
    if (!committed)
    {
        return result_type(unexpect, std::move(committed.error()));
    }
    return result;
}

} // sqlitexx

#endif // SQLITEXX_TRANSACTION_HPP
