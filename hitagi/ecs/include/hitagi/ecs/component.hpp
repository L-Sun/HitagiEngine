#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/types.hpp>

#include <string>
#include <set>
#include <functional>

namespace hitagi::ecs {

template <typename T>
concept Component = std::is_class_v<T> && utils::no_cvref<T> && std::is_default_constructible_v<T>;

struct DynamicComponent {
    std::pmr::string name;
    std::size_t      size;

    std::function<void(std::byte*)> constructor, destructor;

    constexpr auto operator<=>(const DynamicComponent& rhs) const noexcept {
        return name <=> rhs.name;  // use name as key
    };
};
using DynamicComponents = std::pmr::set<DynamicComponent>;

namespace detail {

struct ComponentInfo {
    utils::TypeID type_id;
    std::size_t   size;

    std::function<void(std::byte*)> constructor, destructor;

    std::pmr::string name;

    constexpr auto operator<=>(const ComponentInfo& rhs) const noexcept {
        return std::tie(type_id, size) <=> std::tie(rhs.type_id, rhs.size);
    }
    constexpr auto operator==(const ComponentInfo& rhs) const noexcept {
        return type_id == rhs.type_id && size == rhs.size;
    }
    constexpr auto operator!=(const ComponentInfo& rhs) const noexcept {
        return type_id != rhs.type_id || size != rhs.size;
    }
};
using ComponentInfos = std::pmr::vector<ComponentInfo>;

template <Component... Components>
auto create_component_infos(const DynamicComponents& dynamic_components = {}) noexcept {
    ComponentInfos result = {ComponentInfo{
        .type_id     = utils::TypeID::Create<Components>(),
        .size        = sizeof(Components),
        .constructor = [](std::byte* ptr) { std::construct_at(reinterpret_cast<Components*>(ptr)); },
        .destructor  = [](std::byte* ptr) { std::destroy_at(reinterpret_cast<Components*>(ptr)); },
        .name        = typeid(Components).name(),
    }...};
    for (const auto& dynamic_component : dynamic_components) {
        result.emplace_back(utils::TypeID{dynamic_component.name}, dynamic_component.size, dynamic_component.constructor, dynamic_component.destructor, dynamic_component.name);
    }
    return result;
}
}  // namespace detail
}  // namespace hitagi::ecs

namespace std {
template <>
struct hash<hitagi::ecs::detail::ComponentInfo> {
    std::size_t operator()(const hitagi::ecs::detail::ComponentInfo& info) const noexcept {
        return info.type_id.GetValue();
    }
};
}  // namespace std