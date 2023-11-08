#pragma once
#include <array>
#include <magic_enum.hpp>

namespace hitagi::utils {

template <typename T, std::size_t N>
constexpr std::array<T, N> create_array(T&& value) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{(static_cast<void>(I), std::forward<T>(value))...};
    }
    (std::make_index_sequence<N>{});
}

template <typename T, std::size_t N, typename... Args>
constexpr std::array<T, N> create_array_inplace(Args&&... args) {
    static_assert(std::is_constructible_v<T, Args...>, "Can not construct array inplace");

    auto construct_fn = [&](std::size_t i) -> T {
        return T{std::forward<Args>(args)...};
    };

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{{construct_fn(I)...}};
    }
    (std::make_index_sequence<N>{});
}

template <typename T, typename E>
    requires std::is_enum_v<E>
struct EnumArray : public std::array<T, magic_enum::enum_count<E>()> {
    using array_t = typename std::array<T, magic_enum::enum_count<E>()>;

    T&       operator[](std::size_t index) { return std::array<T, magic_enum::enum_count<E>()>::operator[](index); }
    const T& operator[](std::size_t index) const { return std::array<T, magic_enum::enum_count<E>()>::operator[](index); }
    T&       at(std::size_t index) { return std::array<T, magic_enum::enum_count<E>()>::at(index); }
    const T& at(std::size_t index) const { return std::array<T, magic_enum::enum_count<E>()>::at(index); }

    T&       operator[](const E& e) { return std::array<T, magic_enum::enum_count<E>()>::operator[](magic_enum::enum_integer(e)); }
    const T& operator[](const E& e) const { return std::array<T, magic_enum::enum_count<E>()>::operator[](magic_enum::enum_integer(e)); }
    T&       at(const E& e) { return std::array<T, magic_enum::enum_count<E>()>::at(magic_enum::enum_integer(e)); }
    const T& at(const E& e) const { return std::array<T, magic_enum::enum_count<E>()>::at(magic_enum::enum_integer(e)); }
};

template <typename T, typename E>
    requires std::is_enum_v<E>
constexpr EnumArray<T, E> create_enum_array(T&& initial_value) {
    return {create_array<T, magic_enum::enum_count<E>()>(std::forward<T>(initial_value))};
}
}  // namespace hitagi::utils