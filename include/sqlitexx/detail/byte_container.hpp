#ifndef SQLITEXX_DETAIL_BYTE_CONTAINER_HPP
#define SQLITEXX_DETAIL_BYTE_CONTAINER_HPP

// Which containers hold raw bytes, recognised by their members so that it works in every standard: std::vector,
// std::string, std::array and (C++20) std::span of char, signed char, unsigned char, std::byte (C++17) or char8_t
// (C++20). Containers of wider elements are left out on purpose: their bytes depend on the machine's integer size and
// byte order.

#include <cstddef>
#include <type_traits>
#include <utility>

namespace sqlitexx
{
namespace detail
{
    template <class... T>
    struct make_void
    {
        using type = void;
    };

    template <class T>
    struct is_byte : std::false_type
    {
    };

    template <>
    struct is_byte<char> : std::true_type
    {
    };

    template <>
    struct is_byte<signed char> : std::true_type
    {
    };

    template <>
    struct is_byte<unsigned char> : std::true_type
    {
    };

#if defined(__cpp_lib_byte)
    template <>
    struct is_byte<std::byte> : std::true_type
    {
    };
#endif

#if defined(__cpp_char8_t)
    template <>
    struct is_byte<char8_t> : std::true_type
    {
    };
#endif

    template <class Pointer>
    using pointee = typename std::remove_pointer<Pointer>::type;

    // Bytes to write: data() and size() on a const container, pointing to bytes
    template <class C, class = void>
    struct is_readable_bytes : std::false_type
    {
    };

    template <class C>
    struct is_readable_bytes<
        C,
        typename make_void<decltype(std::declval<const C&>().data()), decltype(std::declval<const C&>().size())>::type>
        : is_byte<typename std::remove_cv<pointee<decltype(std::declval<const C&>().data())>>::type>
    {
    };

    // A container to read into that grows to fit: resize(), size() and operator[] giving a writable byte
    template <class C, class = void>
    struct is_growable_bytes : std::false_type
    {
    };

    template <class C>
    struct is_growable_bytes<
        C,
        typename make_void<
            decltype(std::declval<C&>().resize(std::size_t())),
            decltype(std::declval<C&>().size()),
            decltype(std::declval<C&>()[0])>::type>
        : std::integral_constant<
              bool,
              std::is_lvalue_reference<decltype(std::declval<C&>()[0])>::value
                  && !std::is_const<typename std::remove_reference<decltype(std::declval<C&>()[0])>::type>::value
                  && is_byte<typename std::remove_reference<decltype(std::declval<C&>()[0])>::type>::value>
    {
    };

    // A fixed-size buffer to read into: data() pointing to writable bytes and size(), but no resize()
    template <class C, class = void>
    struct is_fixed_bytes : std::false_type
    {
    };

    template <class C>
    struct is_fixed_bytes<
        C,
        typename make_void<decltype(std::declval<C&>().data()), decltype(std::declval<C&>().size())>::type>
        : std::integral_constant<
              bool,
              !is_growable_bytes<C>::value && !std::is_const<pointee<decltype(std::declval<C&>().data())>>::value
                  && is_byte<pointee<decltype(std::declval<C&>().data())>>::value>
    {
    };

    // C arrays to write from. Arrays of char and char8_t are left out: a string literal would write its '\0' too.
    template <class T>
    struct is_byte_array_element : std::integral_constant<bool, is_byte<T>::value && !std::is_same<T, char>::value>
    {
    };

#if defined(__cpp_char8_t)
    template <>
    struct is_byte_array_element<char8_t> : std::false_type
    {
    };
#endif
} // detail
} // sqlitexx

#endif // SQLITEXX_DETAIL_BYTE_CONTAINER_HPP
