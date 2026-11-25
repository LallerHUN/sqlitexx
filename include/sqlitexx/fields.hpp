#ifndef SQLITEXX_FIELDS_HPP
#define SQLITEXX_FIELDS_HPP

/// \file
/// Rows as your own structs: name the members that columns go into, once, and read the struct wherever a row is read.
///
///     struct user
///     {
///         std::string name;
///         sqlitexx::optional<int> age;
///     };
///     SQLITEXX_FIELDS(user, name, age)
///
///     for (sqlitexx::expected<user>& u : db.query<user>("SELECT name, age FROM user")) { ... }
///     sqlitexx::expected<user> alice = db.query<user>("SELECT * FROM user WHERE id = ?", 1).first();
///
/// Each field is filled from the column with its name, compared like SQL identifiers (ASCII letters in either case),
/// so the order of the columns doesn't matter and SELECT * works. A field without a column is a primary_code::range
/// error that names it; columns without a field are ignored; an alias fits a column to a field
/// (SELECT years AS age). Each field reads through value_traits like a column of its own type: NULL only into an
/// optional field, integers range-checked. The struct must be default-constructible.
///
/// SQLITEXX_FIELDS(Type, members...) goes at namespace scope, in the namespace of the struct, without a semicolon. It
/// takes up to 32 members. It defines a function that sqlitexx finds by argument-dependent lookup, which from C++14 on
/// can also be written by hand, for example to give a field a column name of its own:
///
///     inline auto sqlitexx_fields(const user*)
///     {
///         return sqlitexx::fields(sqlitexx::field("name", &user::name), sqlitexx::field("years", &user::age));
///     }
///
/// A struct is read on its own: query<user> and rows<user>, not alongside other types. A type with fields must not
/// also have value_traits.

#include <tuple>

namespace sqlitexx
{
/// One field: the column name it reads and the member it fills. Made by field().
template <class Class, class Member>
struct field_mapping
{
    const char* name;
    Member Class::* member;
};

/// A field reading the column `name` into `member`
template <
    class Class,
    class Member>
field_mapping<
    Class,
    Member>
field(
    const char* name,
    Member Class::* member
) noexcept
{
    return field_mapping<Class, Member>{name, member};
}

/// The fields of a struct, in the order they are filled
template <class... Fields>
std::tuple<Fields...> fields(Fields... list) noexcept
{
    return std::tuple<Fields...>(list...);
}
} // sqlitexx

/// \cond SQLITEXX_DETAIL
#define SQLITEXX_DETAIL_EXPAND(x) x
#define SQLITEXX_DETAIL_CONCAT(a, b) SQLITEXX_DETAIL_CONCAT_I(a, b)
#define SQLITEXX_DETAIL_CONCAT_I(a, b) a##b
// The number of arguments, up to 32; the trailing 0 keeps the macro's own ... from ever being empty
#define SQLITEXX_DETAIL_COUNT(...) \
    SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_COUNT_N( \
        __VA_ARGS__, \
        32, \
        31, \
        30, \
        29, \
        28, \
        27, \
        26, \
        25, \
        24, \
        23, \
        22, \
        21, \
        20, \
        19, \
        18, \
        17, \
        16, \
        15, \
        14, \
        13, \
        12, \
        11, \
        10, \
        9, \
        8, \
        7, \
        6, \
        5, \
        4, \
        3, \
        2, \
        1, \
        0 \
    ))
#define SQLITEXX_DETAIL_COUNT_N( \
    _1, \
    _2, \
    _3, \
    _4, \
    _5, \
    _6, \
    _7, \
    _8, \
    _9, \
    _10, \
    _11, \
    _12, \
    _13, \
    _14, \
    _15, \
    _16, \
    _17, \
    _18, \
    _19, \
    _20, \
    _21, \
    _22, \
    _23, \
    _24, \
    _25, \
    _26, \
    _27, \
    _28, \
    _29, \
    _30, \
    _31, \
    _32, \
    count, \
    ... \
) \
    count
#define SQLITEXX_DETAIL_FIELD(Type, member) ::sqlitexx::field(#member, &Type::member)
#define SQLITEXX_DETAIL_FIELDS_1(Type, member) SQLITEXX_DETAIL_FIELD(Type, member)
#define SQLITEXX_DETAIL_FIELDS_2(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_1(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_3(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_2(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_4(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_3(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_5(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_4(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_6(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_5(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_7(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_6(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_8(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_7(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_9(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_8(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_10(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_9(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_11(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_10(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_12(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_11(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_13(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_12(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_14(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_13(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_15(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_14(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_16(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_15(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_17(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_16(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_18(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_17(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_19(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_18(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_20(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_19(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_21(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_20(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_22(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_21(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_23(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_22(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_24(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_23(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_25(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_24(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_26(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_25(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_27(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_26(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_28(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_27(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_29(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_28(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_30(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_29(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_31(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_30(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELDS_32(Type, member, ...) \
    SQLITEXX_DETAIL_FIELD(Type, member), SQLITEXX_DETAIL_EXPAND(SQLITEXX_DETAIL_FIELDS_31(Type, __VA_ARGS__))
#define SQLITEXX_DETAIL_FIELD_LIST(Type, ...) \
    SQLITEXX_DETAIL_EXPAND( \
        SQLITEXX_DETAIL_CONCAT(SQLITEXX_DETAIL_FIELDS_, SQLITEXX_DETAIL_COUNT(__VA_ARGS__))(Type, __VA_ARGS__) \
    )
/// \endcond

/// Names the members of `Type` that rows are read into, each from the column with the member's name. At namespace
/// scope, in the namespace of `Type`, without a semicolon.
#define SQLITEXX_FIELDS(Type, ...) \
    inline auto sqlitexx_fields(const Type*) \
        -> decltype(::sqlitexx::fields(SQLITEXX_DETAIL_FIELD_LIST(Type, __VA_ARGS__))) \
    { \
        return ::sqlitexx::fields(SQLITEXX_DETAIL_FIELD_LIST(Type, __VA_ARGS__)); \
    }

#endif // SQLITEXX_FIELDS_HPP
