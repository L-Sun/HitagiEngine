export module utils:enum_utils;
import interop.magic_enum;

import std;

import :array;

export namespace hitagi::utils {

template <typename E>
    requires std::is_enum_v<E>
struct enable_bitmask_operators : public magic_enum::customize::enum_range<E> {};

template <typename E>
concept EnumFlag = enable_bitmask_operators<E>::is_flags;

}  // namespace hitagi::utils

export template <hitagi::utils::EnumFlag E>
constexpr E operator|(E lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

export template <hitagi::utils::EnumFlag E>
constexpr E& operator|=(E& lhs, const E& rhs) {
    using underlying = std::underlying_type_t<E>;
    lhs              = static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

export template <hitagi::utils::EnumFlag E>
constexpr E operator&(E lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

export template <hitagi::utils::EnumFlag E>
constexpr E& operator&=(E& lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    lhs              = static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

export template <hitagi::utils::EnumFlag E>
constexpr E operator~(E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<underlying>(rhs));
}

export namespace hitagi::utils {

template <EnumFlag E>
constexpr bool has_flag(E lhs, E rhs) {
    return (lhs & rhs) == rhs;
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
