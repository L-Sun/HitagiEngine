#pragma once
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/types.hpp>

#include <string>
#include <set>
#include <unordered_set>
#include <functional>

namespace hitagi::ecs {

template <typename T>
concept Component = std::is_class_v<T> && utils::no_cvref<T> && std::is_default_constructible_v<T>;

struct ComponentInfo {
    std::pmr::string name;
    utils::TypeID    type_id;
    std::size_t      size;

    std::function<void(std::byte*)>             constructor, destructor;
    std::function<void(std::byte*, std::byte*)> copy_constructor, move_constructor;

    constexpr auto operator<=>(const ComponentInfo& rhs) const noexcept {
        return std::tie(size, type_id) <=> std::tie(rhs.size, rhs.type_id);
    }
    constexpr auto operator==(const ComponentInfo& rhs) const noexcept {
        return type_id == rhs.type_id;
    }
    constexpr auto operator!=(const ComponentInfo& rhs) const noexcept {
        return type_id != rhs.type_id;
    }
};

using DynamicComponentSet  = std::pmr::unordered_set<std::pmr::string>;
using DynamicComponentList = std::pmr::vector<std::pmr::string>;

namespace detail {

using ComponentInfoSet = std::pmr::set<ComponentInfo>;

template <Component T>
constexpr auto create_static_component_info() noexcept {
    return ComponentInfo{
        .name        = typeid(T).name(),
        .type_id     = utils::TypeID::Create<T>(),
        .size        = sizeof(T),
        .constructor = [](std::byte* ptr) { std::construct_at(reinterpret_cast<T*>(ptr)); },
        .destructor  = [](std::byte* ptr) { std::destroy_at(reinterpret_cast<T*>(ptr)); },
        .copy_constructor =
            [](std::byte* dst, std::byte* src) { std::construct_at(reinterpret_cast<T*>(dst), *reinterpret_cast<T*>(src)); },
        .move_constructor = [](std::byte* dst, std::byte* src) { std::construct_at(reinterpret_cast<T*>(dst), std::move(*reinterpret_cast<T*>(src))); },
    };
}

template <Component... Components>
    requires utils::unique_types<Components...>
auto create_component_info_set(const ComponentInfoSet& dynamic_components = {}) noexcept {
    ComponentInfoSet result = {create_static_component_info<Components>()...};
    for (auto dynamic_component : dynamic_components) {
        dynamic_component.type_id = utils::TypeID(dynamic_component.name);
        result.emplace(dynamic_component);
    }
    return result;
}

using ComponentIdSet  = std::pmr::unordered_set<utils::TypeID>;
using ComponentIdList = std::pmr::vector<utils::TypeID>;

template <Component... Components>
auto create_component_id_set(const DynamicComponentSet& dynamic_components = {}) noexcept {
    ComponentIdSet result = {utils::TypeID::Create<Components>()...};
    for (const auto& dynamic_component : dynamic_components) {
        result.emplace(dynamic_component);
    }
    return result;
}

template <Component... Components>
auto create_component_id_list(const DynamicComponentList& dynamic_components = {}) noexcept {
    ComponentIdList result = {utils::TypeID::Create<Components>()...};
    for (const auto& dynamic_component : dynamic_components) {
        result.emplace_back(dynamic_component);
    }
    return result;
}

}  // namespace detail
}  // namespace hitagi::ecs

namespace std {
template <>
struct hash<hitagi::ecs::ComponentInfo> {
    std::size_t operator()(const hitagi::ecs::ComponentInfo& info) const noexcept {
        return info.type_id.GetValue();
    }
};
}  // namespace std