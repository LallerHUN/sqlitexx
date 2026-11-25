#ifndef SQLITEXX_REGISTRATIONS_HPP
#define SQLITEXX_REGISTRATIONS_HPP

// A token that removes SQL functions, aggregates and collations, and clears hooks, when it ends, so they can refer
// to state outside them that ends before the database does:
//
//     sort_settings settings;
//     sqlitexx::registrations scope;   // after the state: it ends first
//     db.create_collation("configured", [&settings](...) { ... }, scope);
//     db.create_function("set_descending", [&settings](bool descending) { ... }, scope);
//     // the end of the scope removes both, then settings ends
//
// Registering with a token works like registering without one, and SQLite keeps the callable the same way. The
// token adds the end of the scope:
//  - The destructor removes what the token holds, the latest first. remove() does the same now and returns the first
//    error; what it couldn't remove stays in the token.
//  - release() keeps the registrations and forgets them: they stay until replaced, removed or the database closes.
//  - What is gone already needs nothing: replaced by registering the same name (or setting the same kind of hook)
//    again, removed with remove_function or remove_collation, a hook cleared with nullptr, or released because the
//    database closed. The token may outlive the database.
//  - Hooks (sqlitexx/hooks.hpp) can always be cleared, also while statements run: what follows is about functions,
//    aggregates and collations only.
//
// Removing fails with primary_code::busy while a statement on the connection is running (it returned a row and was
// not reset or finished), any statement, not only those using the name. Declaring statements after the token ends
// them first; a token created inside a loop over the rows of another statement can't remove. When the destructor
// can't remove, it ends the callables instead, and SQLite's later calls of them report it:
//  - A function or aggregate fails the statement calling it with primary_code::misuse.
//  - A collation ends the program (std::terminate): it can't fail, and comparing any other way could corrupt an index.
//    To keep this from happening, call remove() before the scope ends and check its result.
//
// Move-only. The token may hold registrations of several databases.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/registration_block.hpp"
#include "sqlitexx/expected.hpp"

namespace sqlitexx
{
class database;

class registrations final
{
public:
    registrations() noexcept = default;

    registrations(registrations&& other) noexcept;
    registrations& operator=(registrations&& other) noexcept;

    registrations(const registrations&) = delete;
    registrations& operator=(const registrations&) = delete;

    // Removes what the token holds, as remove() does; what can't be removed has its callable ended
    ~registrations();

    // Removes what the token holds, the latest first. The first error is returned; what couldn't be removed stays.
    SQLITEXX_DETAIL_NODISCARD expected<void> remove() noexcept;

    // Keeps the registrations and forgets them
    void release() noexcept;

    bool empty() const noexcept;

private:
    friend class database;

    // The latest registration first
    detail::registration_block* latest = nullptr;

    void adopt(detail::registration_block* block) noexcept;

    // The end of the token: removes what it can and ends the callables of the rest
    void end() noexcept;
};

} // sqlitexx

#endif // SQLITEXX_REGISTRATIONS_HPP
