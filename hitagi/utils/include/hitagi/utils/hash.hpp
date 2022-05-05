#pragma once
#include <cstddef>
#include <functional>

namespace hitagi::utils {
template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

}  // namespace hitagi::utils