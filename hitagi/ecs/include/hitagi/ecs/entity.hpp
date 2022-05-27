#pragma once

#include <compare>
#include <cstdint>

namespace hitagi::ecs {
struct Entity {
    std::uint64_t index = 0;

    constexpr      operator const bool() const { return index != static_cast<std::uint64_t>(-1); }
    constexpr auto operator<=>(const Entity&) const = default;
};

}  // namespace hitagi::ecs

namespace std {
template <>
struct hash<hitagi::ecs::Entity> {
    size_t operator()(const hitagi::ecs::Entity& entity) const {
        return static_cast<size_t>(entity.index);
    }
};
}  // namespace std