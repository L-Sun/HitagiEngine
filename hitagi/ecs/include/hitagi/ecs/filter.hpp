#pragma once
#include <hitagi/ecs/component.hpp>
#include <hitagi/utils/types.hpp>

namespace hitagi::ecs {

class Filter {
public:
    template <Component... Components>
    auto All(const DynamicComponents& dynamic_components = {}) noexcept -> Filter&
        requires utils::unique_types<Components...>;

    template <Component... Components>
    auto Any(const DynamicComponents& dynamic_components = {}) noexcept -> Filter&
        requires utils::unique_types<Components...>;

    template <Component... Components>
    auto None(const DynamicComponents& dynamic_components = {}) noexcept -> Filter&
        requires utils::unique_types<Components...>;

private:
    friend class EntityManager;
    auto AddComponentToFilter(detail::ComponentInfos& filter, detail::ComponentInfos new_component_infos) noexcept -> Filter&;

    detail::ComponentInfos m_All;
    detail::ComponentInfos m_Any;
    detail::ComponentInfos m_None;
};

template <Component... Components>
auto Filter::All(const DynamicComponents& dynamic_components) noexcept -> Filter&
    requires utils::unique_types<Components...>
{
    return AddComponentToFilter(m_All, detail::create_component_infos<Components...>(dynamic_components));
}

template <Component... Components>
auto Filter::Any(const DynamicComponents& dynamic_components) noexcept -> Filter&
    requires utils::unique_types<Components...>
{
    return AddComponentToFilter(m_Any, detail::create_component_infos<Components...>(dynamic_components));
}

template <Component... Components>
auto Filter::None(const DynamicComponents& dynamic_components) noexcept -> Filter&
    requires utils::unique_types<Components...>
{
    return AddComponentToFilter(m_None, detail::create_component_infos<Components...>(dynamic_components));
}

}  // namespace hitagi::ecs