// Tests for running out of memory: sqlitexx gives primary_code::nomem errors instead of letting std::bad_alloc out, and
// the calls that cannot throw say so with noexcept.
//
// With exceptions enabled, the global operator new is replaced by one that can be told to refuse allocations: each
// scenario runs again and again, allowed one more allocation each time, until it gets through. Every run must end
// with the right results or with a nomem error; an exception escaping, or a noexcept call ending the program, fails
// the test. SQLite allocates with malloc, so only the C++ side runs out.

#include "check.hpp"
#include "sqlitexx/sqlitexx.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define SQLITEXX_TEST_EXCEPTIONS 1
#else
#define SQLITEXX_TEST_EXCEPTIONS 0
#endif

// MSVC's iterator debugging (Debug builds) gives every std::string a proxy object on the heap, even an empty one, and
// its noexcept constructors end the program when that allocation fails. With memory gone, the standard library stops
// the program itself, so the runs that refuse allocations are left out there.
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL == 2
#define SQLITEXX_TEST_DEBUG_PROXIES 1
#else
#define SQLITEXX_TEST_DEBUG_PROXIES 0
#endif

using sqlitexx::blob;
using sqlitexx::database;
using sqlitexx::error;
using sqlitexx::expected;
using sqlitexx::optional;
using sqlitexx::statement;
using sqlitexx::native::extended_code;
using sqlitexx::native::primary_code;

// A type whose traits make no promise: the calls using it are not noexcept
struct loose
{
    int value;
};

// A type whose reading runs out of memory in the user's own code
struct greedy
{
};

namespace sqlitexx
{
template <>
struct value_traits<loose>
{
    template <class Target>
    static native::result bind(
        Target& stmt,
        int index,
        const loose& value
    )
    {
        return stmt.bind_int(index, value.value);
    }

    template <class Source>
    static expected<loose> read(
        const Source& stmt,
        int index
    )
    {
        return loose{stmt.column_int(index)};
    }
};

template <>
struct value_traits<greedy>
{
    template <class Source>
    static expected<greedy> read(
        const Source&,
        int
    )
    {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
        throw std::bad_alloc();
#else
        return greedy();
#endif
    }
};
} // sqlitexx

// Compile-time checks: noexcept(expression) must hold, or must not
#define SQLITEXX_TEST_NOEXCEPT(...) static_assert(noexcept(__VA_ARGS__), #__VA_ARGS__)
#define SQLITEXX_TEST_NOT_NOEXCEPT(...) static_assert(!noexcept(__VA_ARGS__), #__VA_ARGS__)

// Registering copies the callable: noexcept when the copy cannot throw
struct no_capture
{
    int operator()(int value) const
    {
        return value;
    }
};

// or_else handler: a fallback value for the error
struct zero_on_error
{
    expected<int> operator()(const error&) const
    {
        return 0;
    }
};

struct string_capture
{
    std::string text;

    int operator()(int value) const
    {
        return value;
    }
};

// A collation: sorts backwards
struct backwards
{
    int operator()(
        sqlitexx::string_view first,
        sqlitexx::string_view second
    ) const
    {
        return second.compare(first);
    }
};

struct backwards_named
{
    std::string name;

    int operator()(
        sqlitexx::string_view first,
        sqlitexx::string_view second
    ) const
    {
        return second.compare(first);
    }
};

// Never called: the checks are in unevaluated operands. A string_view comes in ready made: noexcept(...) includes
// converting arguments, and string-view-lite does not mark its constructors noexcept for every compiler.
inline void noexcept_checks(
    database& db,
    statement& stmt,
    sqlitexx::blob_stream& stream,
    const std::string& name,
    sqlitexx::string_view view,
    const no_capture& plain,
    const string_capture& capturing,
    const backwards_named& named_collation
)
{
    // The calls that run none of the user's code
    SQLITEXX_TEST_NOEXCEPT(database::open(":memory:"));
    SQLITEXX_TEST_NOEXCEPT(db.execute(""));
    SQLITEXX_TEST_NOEXCEPT(db.prepare(view));
    SQLITEXX_TEST_NOEXCEPT(db.backup_to(db));
    SQLITEXX_TEST_NOEXCEPT(db.open_blob("t", "c", 1));
    SQLITEXX_TEST_NOEXCEPT(stmt.step());
    SQLITEXX_TEST_NOEXCEPT(stmt.reset());
    SQLITEXX_TEST_NOEXCEPT(stmt.column_index(view));
    SQLITEXX_TEST_NOEXCEPT(stream.write(nullptr, 0));
    SQLITEXX_TEST_NOEXCEPT(sqlitexx::transaction::begin(db));
    SQLITEXX_TEST_NOEXCEPT(sqlitexx::detail::out_of_memory());

    // Binding and reading follow the value_traits: noexcept for the built-in types, also those that allocate
    SQLITEXX_TEST_NOEXCEPT(stmt.bind(1, name, "text", nullptr));
    SQLITEXX_TEST_NOEXCEPT(stmt.execute(2.5, optional<int>()));
    SQLITEXX_TEST_NOEXCEPT(stmt.column<std::string>(0));
    SQLITEXX_TEST_NOEXCEPT(stmt.column<blob>(view));
    SQLITEXX_TEST_NOEXCEPT(stmt.column<optional<std::string>>(0));
    SQLITEXX_TEST_NOEXCEPT(stmt.first<std::string, optional<std::int64_t>>());
    SQLITEXX_TEST_NOT_NOEXCEPT(stmt.bind(loose{1}));
    SQLITEXX_TEST_NOT_NOEXCEPT(stmt.column<loose>(0));
    SQLITEXX_TEST_NOT_NOEXCEPT(stmt.first<std::string, loose>());

    // What is kept is taken by value: an lvalue is copied at the call, which noexcept(...) includes
    SQLITEXX_TEST_NOEXCEPT(db.create_function("f", plain));
    SQLITEXX_TEST_NOEXCEPT(db.create_function("f", no_capture()));
    SQLITEXX_TEST_NOT_NOEXCEPT(db.create_function("f", capturing));
    SQLITEXX_TEST_NOEXCEPT(db.create_function("f", string_capture()));
    SQLITEXX_TEST_NOEXCEPT(db.create_collation("c", backwards()));
    SQLITEXX_TEST_NOT_NOEXCEPT(db.create_collation("c", named_collation));
    SQLITEXX_TEST_NOEXCEPT(db.create_collation("c", backwards_named()));
    SQLITEXX_TEST_NOEXCEPT(db.remove_function("f", 1));
    SQLITEXX_TEST_NOEXCEPT(db.remove_collation("c"));
    SQLITEXX_TEST_NOEXCEPT(sqlitexx::execute_with("Alice", 42));
    SQLITEXX_TEST_NOEXCEPT(sqlitexx::execute_with(std::string()));
    SQLITEXX_TEST_NOT_NOEXCEPT(sqlitexx::execute_with(name));
    SQLITEXX_TEST_NOT_NOEXCEPT(sqlitexx::bind_values(name));
    SQLITEXX_TEST_NOEXCEPT(sqlitexx::for_each_row<int>(plain));
    SQLITEXX_TEST_NOEXCEPT(sqlitexx::on(primary_code::busy, zero_on_error()));
}

#if SQLITEXX_TEST_EXCEPTIONS

namespace
{
bool limited = false;
std::size_t allowed = 0;
bool refused = false;

// Starts refusing allocations once `count` more have been made, and keeps refusing
void allow_allocations(std::size_t count)
{
    allowed = count;
    refused = false;
    limited = true;
}

// Allocations work again; true when one was refused meanwhile
bool stop_limiting()
{
    limited = false;
    return refused;
}
} // anonymous

void* operator new(std::size_t size)
{
    if (limited)
    {
        if (allowed == 0)
        {
            refused = true;
            throw std::bad_alloc();
        }
        --allowed;
    }
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }
    return memory;
}

void* operator new(
    std::size_t size,
    const std::nothrow_t&
) noexcept
{
    try
    {
        return operator new(size);
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t size)
{
    return operator new(size);
}

void* operator new[](
    std::size_t size,
    const std::nothrow_t& tag
) noexcept
{
    return operator new(size, tag);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(
    void* memory,
    const std::nothrow_t&
) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](
    void* memory,
    const std::nothrow_t&
) noexcept
{
    std::free(memory);
}

#if defined(__cpp_sized_deallocation)
void operator delete(
    void* memory,
    std::size_t
) noexcept
{
    std::free(memory);
}

void operator delete[](
    void* memory,
    std::size_t
) noexcept
{
    std::free(memory);
}
#endif

namespace
{
enum class outcome
{
    completed,
    out_of_memory,
    wrong
};

// Copying these allocates nothing: what sqlitexx copies is tested, not the copy constructors of the user's types
struct greeter
{
    const std::string* prefix;

    std::string operator()(const std::string& name) const
    {
        return *prefix + name;
    }
};

struct joined
{
    const std::string* separator;
    std::string text;

    void step(const std::string& value)
    {
        text += text.empty() ? value : *separator + value;
    }

    std::string finish() const
    {
        return text;
    }
};

// The values the scenarios work with, made before allocations are limited: only sqlitexx allocates while they run
struct inputs
{
    std::string long_name = std::string(100, 'n');
    blob data = blob(300, 0xab);
    std::string prefix = std::string(40, 'p');
    std::string separator = std::string(30, ',');
    greeter greet = greeter{&prefix};
    joined prototype = joined{&separator, std::string()};
};

// True when `result` failed; `out` then says how
template <class T>
bool failed(
    const expected<T>& result,
    outcome& out
)
{
    if (result)
    {
        return false;
    }
    out = result.error().primary() == primary_code::nomem ? outcome::out_of_memory : outcome::wrong;
    return true;
}

// True unless `result` failed with `code` (the message may be missing: copying it can run out too)
template <class T>
bool not_failed_with(
    const expected<T>& result,
    extended_code code,
    outcome& out
)
{
    if (result)
    {
        out = outcome::wrong;
        return true;
    }
    if (result.error().primary() == primary_code::nomem)
    {
        out = outcome::out_of_memory;
        return true;
    }
    if (result.error().code() != code)
    {
        out = outcome::wrong;
        return true;
    }
    return false;
}

template <class T>
bool not_failed_with(
    const expected<T>& result,
    primary_code code,
    outcome& out
)
{
    if (result)
    {
        out = outcome::wrong;
        return true;
    }
    if (result.error().primary() == primary_code::nomem)
    {
        out = outcome::out_of_memory;
        return true;
    }
    if (result.error().primary() != code)
    {
        out = outcome::wrong;
        return true;
    }
    return false;
}

outcome statements(const inputs& in)
{
    outcome out = outcome::completed;
    expected<database> db = database::open(":memory:");
    if (failed(db, out))
    {
        return out;
    }
    if (failed(db->execute("CREATE TABLE t(name TEXT UNIQUE, data BLOB, n INTEGER)"), out))
    {
        return out;
    }

    expected<statement> insert = db->prepare("INSERT INTO t VALUES(?, ?, ?)");
    if (failed(insert, out))
    {
        return out;
    }
    if (failed(insert->execute(in.long_name, in.data, 1), out) || failed(insert->execute("short", blob(), 2), out))
    {
        return out;
    }
    if (not_failed_with(insert->execute(in.long_name, nullptr, 3), extended_code::constraint_unique, out))
    {
        return out;
    }

    // One row, several columns
    expected<statement> query = db->prepare("SELECT name, n FROM t ORDER BY n");
    if (failed(query, out))
    {
        return out;
    }
    expected<std::tuple<std::string, optional<std::int64_t>>> first =
        query->first<std::string, optional<std::int64_t>>();
    if (failed(first, out))
    {
        return out;
    }
    if (std::get<0>(*first) != in.long_name || std::get<1>(*first) != std::int64_t(1))
    {
        return outcome::wrong;
    }

    // By name, and a name that is not there
    expected<statement> by_name = db->prepare("SELECT data, name FROM t ORDER BY n");
    if (failed(by_name, out))
    {
        return out;
    }
    expected<bool> stepped = by_name->step();
    if (failed(stepped, out))
    {
        return out;
    }
    expected<blob> read = by_name->column<blob>("DATA");
    if (failed(read, out))
    {
        return out;
    }
    if (*read != in.data)
    {
        return outcome::wrong;
    }
    if (not_failed_with(by_name->column<std::string>("missing"), primary_code::range, out))
    {
        return out;
    }

    // Every row, through for_each and through rows()
    int seen = 0;
    expected<statement> all = db->prepare("SELECT name, data FROM t ORDER BY n");
    if (failed(all, out))
    {
        return out;
    }
    expected<void> each = all->for_each<std::string, blob>(
        [&](const std::string& name, const blob& value)
        {
            if ((seen == 0 && name == in.long_name && value == in.data) || (seen == 1 && name == "short"))
            {
                ++seen;
            }
        }
    );
    if (failed(each, out))
    {
        return out;
    }
    if (failed(all->reset(), out))
    {
        return out;
    }
    for (expected<std::string>& name : all->rows<std::string>())
    {
        if (failed(name, out))
        {
            return out;
        }
        ++seen;
    }
    if (seen != 4)
    {
        return outcome::wrong;
    }

    // NULL into a type that is no optional
    expected<statement> nulls = db->prepare("SELECT NULL");
    if (failed(nulls, out))
    {
        return out;
    }
    if (not_failed_with(nulls->first<std::string>(), primary_code::mismatch, out))
    {
        return out;
    }
    return outcome::completed;
}

outcome blobs(const inputs& in)
{
    outcome out = outcome::completed;
    expected<database> db = database::open(":memory:");
    if (failed(db, out))
    {
        return out;
    }
    expected<statement> insert = db->prepare("CREATE TABLE b(data BLOB)");
    if (failed(insert, out) || failed(insert->execute(), out))
    {
        return out;
    }
    insert = db->prepare("INSERT INTO b(rowid, data) VALUES(1, ?)");
    if (failed(insert, out) || failed(insert->execute(in.data), out))
    {
        return out;
    }

    expected<sqlitexx::blob_stream> stream = db->open_blob("b", "data", 1, sqlitexx::blob_access::read_write);
    if (failed(stream, out))
    {
        return out;
    }
    std::vector<unsigned char> all;
    expected<std::size_t> got = stream->read(all);
    if (failed(got, out))
    {
        return out;
    }
    if (*got != in.data.size() || all != in.data)
    {
        return outcome::wrong;
    }
    std::string text;
    if (failed(stream->seek(0), out))
    {
        return out;
    }
    got = stream->read(text, 10);
    if (failed(got, out))
    {
        return out;
    }
    if (*got != 10 || text.size() != 10)
    {
        return outcome::wrong;
    }
    std::array<unsigned char, 4> small = {{1, 2, 3, 4}};
    if (not_failed_with(stream->read(small, 5), primary_code::range, out))
    {
        return out;
    }
    if (not_failed_with(stream->seek(1000), primary_code::range, out))
    {
        return out;
    }
    if (failed(stream->seek(0), out) || failed(stream->write(small), out))
    {
        return out;
    }
    if (not_failed_with(stream->write(in.data), primary_code::range, out))
    {
        return out;
    }
    if (failed(stream->close(), out))
    {
        return out;
    }
    return outcome::completed;
}

outcome functions(const inputs& in)
{
    outcome out = outcome::completed;
    expected<database> db = database::open(":memory:");
    if (failed(db, out))
    {
        return out;
    }
    // sqlitexx copies the function, the prototype and the collation, and their strings allocate
    if (failed(db->create_function("greet", in.greet), out) || failed(db->create_aggregate("joined", in.prototype), out)
        || failed(db->create_collation("backwards", backwards()), out))
    {
        return out;
    }

    expected<statement> query = db->prepare(
        "SELECT greet('Ann'), (SELECT joined(x) FROM (SELECT 'a' x UNION SELECT 'b')),"
        " (SELECT x FROM (SELECT 'a' x UNION ALL SELECT 'b') ORDER BY x COLLATE backwards LIMIT 1)"
    );
    if (failed(query, out))
    {
        return out;
    }
    expected<std::tuple<std::string, std::string, std::string>> row =
        query->first<std::string, std::string, std::string>();
    if (failed(row, out))
    {
        return out;
    }
    // Compared without allocating
    const std::string& greeting = std::get<0>(*row);
    if (greeting.size() != in.prefix.size() + 3 || greeting.compare(0, in.prefix.size(), in.prefix) != 0
        || std::get<1>(*row).size() != in.separator.size() + 2 || std::get<2>(*row).size() != 1
        || std::get<2>(*row)[0] != 'b')
    {
        return outcome::wrong;
    }
    return outcome::completed;
}

outcome transactions_and_backups(const inputs& in)
{
    outcome out = outcome::completed;
    expected<database> db = database::open(":memory:");
    if (failed(db, out))
    {
        return out;
    }
    expected<std::int64_t> id = sqlitexx::in_transaction(
        *db,
        [&]() -> expected<std::int64_t>
        {
            return db->execute("CREATE TABLE t(name TEXT)")
                .and_then(
                    [&]
                    {
                        return db->prepare("INSERT INTO t VALUES(?)");
                    }
                )
                .and_then(sqlitexx::execute_with(sqlitexx::string_view(in.long_name)))
                .transform(
                    [&]
                    {
                        return db->last_insert_rowid();
                    }
                );
        }
    );
    if (failed(id, out))
    {
        return out;
    }

    expected<database> copy = database::open(":memory:");
    if (failed(copy, out) || failed(db->backup_to(*copy), out))
    {
        return out;
    }
    expected<statement> query = copy->prepare("SELECT name FROM t");
    if (failed(query, out))
    {
        return out;
    }
    expected<std::string> name = query->first<std::string>();
    if (failed(name, out))
    {
        return out;
    }
    return *name == in.long_name ? outcome::completed : outcome::wrong;
}

// Runs `scenario` with 0, 1, 2, ... allocations allowed until it gets through
void exhaust(
    const char* name,
    outcome (*scenario)(const inputs&)
)
{
    const inputs in;
    for (std::size_t count = 0;; ++count)
    {
        outcome result = outcome::wrong;
        bool escaped = false;
        allow_allocations(count);
        try
        {
            result = scenario(in);
        }
        catch (...)
        {
            escaped = true;
        }
        const bool ran_out = stop_limiting();

        if (escaped)
        {
            std::printf("%s: an exception escaped with %u allocations allowed\n", name, static_cast<unsigned>(count));
            CHECK(!escaped);
            return;
        }
        if (result == outcome::wrong)
        {
            std::printf("%s: wrong result with %u allocations allowed\n", name, static_cast<unsigned>(count));
            CHECK(result != outcome::wrong);
            return;
        }
        if (result == outcome::out_of_memory && !ran_out)
        {
            std::printf("%s: nomem without running out, with %u allowed\n", name, static_cast<unsigned>(count));
            CHECK(ran_out);
            return;
        }
        if (result == outcome::completed && !ran_out)
        {
            // It needed `count` allocations, so every one of them was refused once
            CHECK(count > 0);
            return;
        }
    }
}

// sqlitexx catches only its own allocations: what the user's traits throw reaches the caller, and the statement stays
// usable
void test_user_exceptions_pass_through()
{
    expected<database> db = database::open(":memory:");
    CHECK(db.has_value());
    if (!db)
    {
        return;
    }
    expected<statement> query = db->prepare("SELECT 1");
    CHECK(query && query->step().has_value());
    if (!query)
    {
        return;
    }
    bool caught = false;
    try
    {
        expected<greedy> never = query->column<greedy>(0);
        CHECK(!never.has_value());
    }
    catch (const std::bad_alloc&)
    {
        caught = true;
    }
    CHECK(caught);
    expected<int> one = query->column<int>(0);
    CHECK(one && *one == 1);
}

// A function rather than the macro in an if: compilers warn about constant conditions
bool allocations_can_be_refused()
{
    return SQLITEXX_TEST_DEBUG_PROXIES == 0;
}

void test_out_of_memory_error_allocates_nothing()
{
    allow_allocations(0);
    const error e = sqlitexx::detail::out_of_memory();
    const bool ran_out = stop_limiting();
    CHECK(!ran_out);
    CHECK(e.primary() == primary_code::nomem);
    CHECK(e.message() == "out of memory");
}
} // anonymous

#endif

int main()
{
#if SQLITEXX_TEST_EXCEPTIONS
    test_user_exceptions_pass_through();
    if (allocations_can_be_refused())
    {
        test_out_of_memory_error_allocates_nothing();
        exhaust("statements", statements);
        exhaust("blobs", blobs);
        exhaust("functions", functions);
        exhaust("transactions and backups", transactions_and_backups);
    }
    else
    {
        std::printf("MSVC iterator debugging: the runs that refuse allocations are left out\n");
    }
#endif
    return test::summary();
}
