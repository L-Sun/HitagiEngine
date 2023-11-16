#pragma once
#include <hitagi/ecs/component.hpp>
#include <hitagi/utils/types.hpp>

namespace hitagi::ecs {

struct Filter {
    template <Component... Components>
    auto All(const DynamicComponents& dynamic_components = {}) noexcept -> Filter&
        requires utils::unique_types<Components...>;

    template <Component... Components>
    auto Any(const DynamicComponents& dynamic_components = {}) noexcept -> Filter&
        requires utils::unique_types<Components...>;

    template <Component... Components>
    auto None(const DynamicComponents& dynamic_components = {}) noexcept -> Filter&
        requires utils::unique_types<Components...>;

    std::pmr::set<utils::TypeID> all;
    std::pmr::set<utils::TypeID> any;
    std::pmr::set<utils::TypeID> none;
};

template <Component... Components>
auto Filter::All(const DynamicComponents& dynamic_components) noexcept -> Filter&
    requires utils::unique_types<Components...>
{
    (all.emplace(utils::TypeID::Create<Components>()), ...);
    for (const auto& dynamic_component : dynamic_components) {
        all.emplace(utils::TypeID{dynamic_component.name});
    }
    return *this;
}

template <Component... Components>
auto Filter::Any(const DynamicComponents& dynamic_components) noexcept -> Filter&
    requires utils::unique_types<Components...>
{
    (any.emplace(utils::TypeID::Create<Components>()), ...);
    for (const auto& dynamic_component : dynamic_components) {
        any.emplace(utils::TypeID{dynamic_component.name});
    }
    return *this;
}

template <Component... Components>
auto Filter::None(const DynamicComponents& dynamic_components) noexcept -> Filter&
    requires utils::unique_types<Components...>
{
    (none.emplace(utils::TypeID::Create<Components>()), ...);
    for (const auto& dynamic_component : dynamic_components) {
        none.emplace(utils::TypeID{dynamic_component.name});
    }
    return *this;
}

}  // namespace hitagi::ecs