#pragma once
#include <array>
#include <type_traits>

namespace hitagi::utils {
template <typename T, std::size_t N>
constexpr std::array<T, N> create_array(const T& value) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{(static_cast<void>(I), value)...};
    }
    (std::make_index_sequence<N>{});
}

template <typename T, std::size_t N, typename... Args>
constexpr std::array<T, N> create_array_inplcae(Args&&... args) {
    static_assert(std::is_constructible_v<T, Args...>, "Can not construct array inplace");

    auto construct_fn = [&](std::size_t i) -> T {
        return T{args...};
    };

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{{construct_fn(I)...}};
    }
    (std::make_index_sequence<N>{});
}

constexpr size_t align(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

}  // namespace hitagi::utils