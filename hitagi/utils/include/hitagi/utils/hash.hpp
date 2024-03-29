#pragma once
#include <cstddef>
#include <functional>
#include <string_view>
#include <set>

namespace hitagi::utils {
template <typename T>
constexpr inline std::size_t hash(const T& obj) noexcept {
    return std::hash<T>{}(obj);
}

template <std::size_t N>
constexpr inline std::size_t combine_hash(const std::array<std::size_t, N>& hash_values, std::size_t seed = 0) noexcept {
    for (std::size_t hash : hash_values) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

constexpr inline std::size_t combine_hash(const std::pmr::vector<std::size_t>& hash_values, std::size_t seed = 0) noexcept {
    for (std::size_t hash : hash_values) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

inline std::size_t combine_hash(const std::pmr::set<std::size_t>& hash_values, std::size_t seed = 0) noexcept {
    for (std::size_t hash : hash_values) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) noexcept {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

template <typename T1, typename T2>
constexpr inline std::size_t combine_hash(const T1& a, const T2& b, std::size_t seed = 0) noexcept {
    return combine_hash(seed, a, b);
}

namespace detail {
template <unsigned bytesize>
struct fnv1a_traits;
template <>
struct fnv1a_traits<4> {
    using type                            = std::uint32_t;
    static constexpr std::uint32_t offset = 2166136261;
    static constexpr std::uint32_t prime  = 16777619;
};
template <>
struct fnv1a_traits<8> {
    using type                            = std::uint64_t;
    static constexpr std::uint64_t offset = 14695981039346656037ull;
    static constexpr std::uint64_t prime  = 1099511628211ull;
};
}  // namespace detail

constexpr inline std::size_t string_hash(std::string_view str) noexcept {
    using Traits      = detail::fnv1a_traits<sizeof(std::size_t)>;
    std::size_t value = Traits::offset;
    for (const char& c : str) {
        value = (value ^ static_cast<Traits::type>(c)) * Traits::prime;
    }
    return value;
}

}  // namespace hitagi::utils