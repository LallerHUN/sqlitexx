#ifndef SQLITEXX_DETAIL_AGGREGATE_CALL_HPP
#define SQLITEXX_DETAIL_AGGREGATE_CALL_HPP

// What SQLite calls for an aggregate made from a C++ class: one copy of the prototype per group, created on the
// group's first row, fed with step() (and for window functions taken back with inverse()), asked for its result with
// finish() (or value() while a window moves) and destroyed after the final call.

#include "sqlitexx/config.hpp"
#include "sqlitexx/detail/sql_call.hpp"
#include "sqlitexx/detail/utility.hpp"
#include "sqlitexx/native/context.hpp"

#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    // An aggregate class: step(...) and finish()
    template <class T, class = void>
    struct is_aggregate_class : std::false_type
    {
    };

    template <class T>
    struct is_aggregate_class<T, typename make_void<decltype(&T::step), decltype(&T::finish)>::type> : std::true_type
    {
    };

    // A window function class: also inverse(...) and value()
    template <class T, class = void>
    struct is_window_class : std::false_type
    {
    };

    template <class T>
    struct is_window_class<T, typename make_void<decltype(&T::inverse), decltype(&T::value)>::type> : std::true_type
    {
    };

    template <class T>
    class aggregate_caller
    {
    public:
        using arguments = typename callable_signature<decltype(&T::step)>::arguments;

        static const int argument_count = static_cast<int>(std::tuple_size<arguments>::value);

        static void step(
            sqlite3_context* handle,
            int,
            sqlite3_value** values
        ) noexcept
        {
            native::context ctx(handle);
            guarded(
                ctx,
                [&]
                {
                    T* object = current(ctx, true);
                    if (object == nullptr)
                    {
                        return;
                    }
                    arguments read;
                    if (!argument_reader<arguments>::read(ctx, values, read) || !feed(ctx, *object, read, step_tag()))
                    {
                        mark_failed(ctx);
                    }
                }
            );
        }

        static void inverse(
            sqlite3_context* handle,
            int,
            sqlite3_value** values
        ) noexcept
        {
            native::context ctx(handle);
            guarded(
                ctx,
                [&]
                {
                    T* object = current(ctx, true);
                    if (object == nullptr)
                    {
                        return;
                    }
                    arguments read;
                    if (!argument_reader<arguments>::read(ctx, values, read)
                        || !feed(ctx, *object, read, inverse_tag()))
                    {
                        mark_failed(ctx);
                    }
                }
            );
        }

        static void value(sqlite3_context* handle) noexcept
        {
            native::context ctx(handle);
            guarded(
                ctx,
                [&]
                {
                    answer(
                        ctx,
                        [](T& object)
                        {
                            return object.value();
                        }
                    );
                }
            );
        }

        static void finish(sqlite3_context* handle) noexcept
        {
            native::context ctx(handle);
            // Whatever happens, the group's object goes with the final call
            const release_on_exit release(state_of(ctx));
            guarded(
                ctx,
                [&]
                {
                    answer(
                        ctx,
                        [](T& object)
                        {
                            return object.finish();
                        }
                    );
                }
            );
        }

        static void destroy(void* prototype) noexcept
        {
            delete static_cast<T*>(prototype);
        }

    private:
        // What SQLite keeps per group, in its zero-filled aggregate memory
        struct state
        {
            T* object;
            bool failed;
        };

        struct step_tag
        {
        };

        // A member function bound to its object, called like a function
        template <class Member>
        struct bound_member
        {
            T& object;
            Member member;

            template <class... A>
            auto operator()(A&&... a) const
                -> decltype((std::declval<T&>().*std::declval<Member>())(std::forward<A>(a)...))
            {
                return (object.*member)(std::forward<A>(a)...);
            }
        };

        struct inverse_tag
        {
        };

        class release_on_exit
        {
        public:
            explicit release_on_exit(state* held) noexcept
                : group(held)
            {
            }

            release_on_exit(const release_on_exit&) = delete;
            release_on_exit& operator=(const release_on_exit&) = delete;

            ~release_on_exit()
            {
                if (group != nullptr)
                {
                    delete group->object;
                    group->object = nullptr;
                }
            }

        private:
            state* group;
        };

        // The group's state without allocating it: null when no row has reached the group
        static state* state_of(native::context& ctx) noexcept
        {
            return static_cast<state*>(ctx.aggregate_context(0));
        }

        // The group's object, created from the prototype on the group's first row. Null when the call has failed
        // (out of memory) or an earlier row of the group failed.
        static T* current(
            native::context& ctx,
            bool create
        )
        {
            state* group = static_cast<state*>(ctx.aggregate_context(create ? sizeof(state) : 0));
            if (group == nullptr)
            {
                if (create)
                {
                    ctx.result_error_nomem();
                }
                return nullptr;
            }
            if (group->failed)
            {
                return nullptr;
            }
            if (group->object == nullptr)
            {
                const T& prototype = *static_cast<const T*>(ctx.user_data());
                group->object = new (std::nothrow) T(prototype);
                if (group->object == nullptr)
                {
                    ctx.result_error_nomem();
                    group->failed = true;
                }
            }
            return group->object;
        }

        static void mark_failed(native::context& ctx) noexcept
        {
            state* group = state_of(ctx);
            if (group != nullptr)
            {
                group->failed = true;
            }
        }

        static bool feed(
            native::context& ctx,
            T& object,
            arguments& read,
            step_tag
        )
        {
            return result_writer::proceed(
                ctx,
                [&]
                {
                    return detail::apply(bound_member<decltype(&T::step)>{object, &T::step}, std::move(read));
                }
            );
        }

        static bool feed(
            native::context& ctx,
            T& object,
            arguments& read,
            inverse_tag
        )
        {
            return result_writer::proceed(
                ctx,
                [&]
                {
                    return detail::apply(bound_member<decltype(&T::inverse)>{object, &T::inverse}, std::move(read));
                }
            );
        }

        // The result: from the group's object, or for a group no row reached (an empty table) from a fresh copy of
        // the prototype, as SQL gives a result for zero rows too. Nothing after a failed row: that error stands.
        template <class Ask>
        static void answer(
            native::context& ctx,
            Ask ask
        )
        {
            state* group = state_of(ctx);
            if (group != nullptr && group->failed)
            {
                return;
            }
            if (group != nullptr && group->object != nullptr)
            {
                T& object = *group->object;
                result_writer::respond(
                    ctx,
                    [&]
                    {
                        return ask(object);
                    }
                );
                return;
            }
            T fresh(*static_cast<const T*>(ctx.user_data()));
            result_writer::respond(
                ctx,
                [&]
                {
                    return ask(fresh);
                }
            );
        }
    };
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_AGGREGATE_CALL_HPP
