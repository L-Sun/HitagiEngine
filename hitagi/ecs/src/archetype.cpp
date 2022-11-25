#include <hitagi/ecs/archetype.hpp>

namespace hitagi::ecs::detials {

auto IArchetype::GetDynamicCompoentArray(std::string_view name) -> std::pair<void*, std::size_t> {
    return {GetComponentRawData(m_ComponentIndexMap.at(utils::TypeID{name})), NumEntities()};
}

auto IArchetype::GetDynamicComponent(Entity entity, std::string_view name) -> void* {
    const std::size_t index = GetEntityIndex(entity);
    if (index == NumEntities()) return nullptr;

    return GetComponentRawData(m_ComponentIndexMap.at(utils::TypeID{name}), index);
}

auto IArchetype::GetEntityIndex(Entity entity) const -> std::size_t {
    return m_EntityMap.contains(entity) ? m_EntityMap.at(entity) : m_EntityMap.size();
}

}  // namespace hitagi::ecs::detials