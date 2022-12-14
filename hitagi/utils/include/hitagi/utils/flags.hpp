#pragma once
#include <type_traits>

namespace hitagi::utils {

// Enable enum flags
template <typename E>
    requires std::is_enum_v<E>
struct enable_bitmask_operators {
    static constexpr bool enable = false;
};

template <typename E>
concept EnumFlag = enable_bitmask_operators<E>::enable;

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