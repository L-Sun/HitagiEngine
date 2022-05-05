#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/archetype.hpp>

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hitagi::ecs {
class Schedule;

template <typename T>
concept SystemLike = requires(Schedule& s) {
    T::OnUpdate(s);
};

class World {
public:
    void Update();

    template <SystemLike System>
    void RegisterSystem();

    // create a entity that has multiple components.
    template <typename... Components>
    requires utils::unique_types<Components...>
        Entity CreateEntity();

    void DestoryEntity(const Entity& entity);

    // Get a array of archetype which contains the interseted components
    template <typename... Components>
    requires utils::unique_types<Components...>
        std::vector<std::shared_ptr<IArchetype>> GetArchetypes();

    template <typename Component>
    std::optional<std::reference_wrapper<Component>> AccessEntity(const Entity& entity);

private:
    std::size_t                                                       m_Counter = 0;
    std::pmr::unordered_map<Entity, std::shared_ptr<IArchetype>>      m_EnitiesMap;
    std::pmr::unordered_map<ArchetypeId, std::shared_ptr<IArchetype>> m_Archetypes;

    std::pmr::vector<std::function<void(Schedule&)>> m_Systems;
};

template <SystemLike System>
void World::RegisterSystem() {
    m_Systems.emplace_back(&System::OnUpdate);
}

template <typename... Components>
requires utils::unique_types<Components...>
    Entity World::CreateEntity() {
    auto id = get_archetype_id<Components...>();
    if (m_Archetypes.count(id) == 0) {
        m_Archetypes.emplace(id, Archetype<Components...>::Create());
    }

    auto archetype         = m_Archetypes.at(id);
    auto&& [iter, success] = m_EnitiesMap.emplace(Entity{.index = m_Counter++}, archetype);
    archetype->CreateInstance(iter->first);

    return iter->first;
}

template <typename Component>
auto World::AccessEntity(const Entity& entity) -> std::optional<std::reference_wrapper<Component>> {
    if (m_EnitiesMap.count(entity) == 0) {
        return std::nullopt;
    }
    return m_EnitiesMap.at(entity)->GetComponent<Component>(entity);
}

template <typename... Components>
requires utils::unique_types<Components...>
    std::vector<std::shared_ptr<IArchetype>> World::GetArchetypes() {
    std::vector<std::shared_ptr<IArchetype>> result;
    for (auto& [id, archetype] : m_Archetypes) {
        auto has_components = (true && ... && archetype->HasComponents<Components>());
        if (has_components)
            result.emplace_back(archetype);
    }
    return result;
}

}  // namespace hitagi::ecs