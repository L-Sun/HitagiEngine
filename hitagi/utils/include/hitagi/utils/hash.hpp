#pragma once
#include <cstddef>
#include <functional>

namespace hitagi::utils {
template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

template <typename T1, typename T2>
inline std::size_t hash_combine(const T1& a, const T2& b, std::size_t seed = 0) {
    hash_combine(seed, a, b);
    return seed;
}

}  // namespace hitagi::utils