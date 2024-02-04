#include "archetype.hpp"
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/entity_manager.hpp>

#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

namespace hitagi::ecs {
Entity::operator bool() const noexcept {
    return m_EntityManager && m_EntityManager->m_EntityMaps.contains(*this);
}

bool Entity::HasComponent(std::string_view dynamic_component) const noexcept {
    return HasComponent(utils::TypeID{dynamic_component});
}

bool Entity::HasComponent(utils::TypeID component_id) const noexcept {
    return GetArchetype().HasComponent(component_id);
}

auto Entity::GetComponent(std::string_view dynamic_component) const -> std::byte* {
    return GetComponent(utils::TypeID{dynamic_component});
}

auto Entity::GetComponent(utils::TypeID component_id) const noexcept -> std::byte* {
    return GetArchetype().GetComponentData(*this, component_id);
}

void Entity::Attach(detail::ComponentInfoSet component_infos, const DynamicComponentSet& dynamic_components) {
    for (const auto& dynamic_component : dynamic_components) {
        component_infos.emplace(m_EntityManager->GetComponentInfo(dynamic_component));
    }
    m_EntityManager->Attach(*this, component_infos);
}

void Entity::Detach(detail::ComponentInfoSet component_infos, const DynamicComponentSet& dynamic_components) {
    for (const auto& dynamic_component : dynamic_components) {
        component_infos.emplace(m_EntityManager->GetComponentInfo(dynamic_component));
    }
    m_EntityManager->Detach(*this, component_infos);
}

auto Entity::GetArchetype() const noexcept -> Archetype& {
    return *m_EntityManager->m_EntityMaps.at(*this);
}

}  // namespace hitagi::ecs