#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace hitagi::ecs {

struct Entity {
    std::uint64_t id = std::numeric_limits<std::uint64_t>::max();

    static constexpr Entity InvalidEntity() {
        return {.id = std::numeric_limits<std::uint64_t>::max()};
    }
    constexpr      operator const bool() const { return id != std::numeric_limits<std::uint64_t>::max(); }
    constexpr auto operator<=>(const Entity&) const = default;
};

}  // namespace hitagi::ecs

namespace std {
template <>
struct hash<hitagi::ecs::Entity> {
    constexpr size_t operator()(const hitagi::ecs::Entity& entity) const {
        return static_cast<size_t>(entity.id);
    }
};
}  // namespace std