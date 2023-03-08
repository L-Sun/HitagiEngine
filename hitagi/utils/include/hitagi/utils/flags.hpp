#pragma once
#include <type_traits>

#include <magic_enum.hpp>

namespace hitagi::utils {

// Enable enum flags
template <typename E>
    requires std::is_enum_v<E>
struct enable_bitmask_operators : public magic_enum::customize::enum_range<E> {};

template <typename E>
concept EnumFlag = enable_bitmask_operators<E>::is_flags;

}  // namespace hitagi::utils

template <hitagi::utils::EnumFlag E>
constexpr E operator|(E lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <hitagi::utils::EnumFlag E>
constexpr E& operator|=(E& lhs, const E& rhs) {
    using underlying = std::underlying_type_t<E>;
    lhs              = static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

template <hitagi::utils::EnumFlag E>
constexpr E operator&(E lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <hitagi::utils::EnumFlag E>
constexpr E& operator&=(E& lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    lhs              = static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

template <hitagi::utils::EnumFlag E>
constexpr E operator~(E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<underlying>(rhs));
}
namespace hitagi::utils {
template <EnumFlag E>
constexpr bool has_flag(E lhs, E rhs) {
    return (lhs & rhs) == rhs;
}
}  // namespace hitagi::utils