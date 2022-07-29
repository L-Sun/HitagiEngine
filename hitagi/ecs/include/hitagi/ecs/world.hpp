#pragma once
#include <algorithm>
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/archetype.hpp>
#include <hitagi/core/timer.hpp>

#include <list>
#include <memory>
#include <memory_resource>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hitagi::ecs {
class Schedule;

template <typename T>
concept SystemLike = requires(Schedule& s, std::chrono::duration<double> delta) {
    T::OnUpdate(s, delta);
};

class World {
public:
    World();

    void Update();

    template <SystemLike System>
    void RegisterSystem();

    inline std::size_t NumEntities() const noexcept { return m_EnitiesMap.size(); }

    // create an entity that has multiple components.
    template <typename... Components>
    Entity CreateEntity() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    template <typename... Components>
    std::pmr::vector<Entity> CreateEntities(std::size_t num) requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    void DestoryEntity(const Entity& entity);

    // Get a array of archetype which contains the interseted components
    template <typename... Components>
    std::vector<std::shared_ptr<IArchetype>> GetArchetypes() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    template <typename Component>
    std::optional<std::reference_wrapper<Component>> AccessEntity(const Entity& entity) requires utils::NoCVRef<Component>;

    template <typename Component>
    bool HasEntity(const Entity& entity) requires utils::NoCVRef<Component>;

private:
    core::Clock                                                       m_Timer;
    std::size_t                                                       m_Counter = 0;
    std::pmr::unordered_map<Entity, std::shared_ptr<IArchetype>>      m_EnitiesMap;
    std::pmr::unordered_map<ArchetypeId, std::shared_ptr<IArchetype>> m_Archetypes;

    std::pmr::vector<std::function<void(Schedule&, std::chrono::duration<double>)>> m_Systems;
};

template <SystemLike System>
void World::RegisterSystem() {
    m_Systems.emplace_back(&System::OnUpdate);
}

template <typename... Components>
Entity World::CreateEntity() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    auto id = get_archetype_id<Components...>();
    if (m_Archetypes.count(id) == 0) {
        m_Archetypes.emplace(id, Archetype<Components...>::Create());
    }

    std::shared_ptr<IArchetype> archetype = m_Archetypes.at(id);

    auto&& [iter, success] = m_EnitiesMap.emplace(Entity{.id = m_Counter++}, archetype);
    assert(success);
    archetype->CreateInstances({iter->first});

    return iter->first;
}

template <typename... Components>
std::pmr::vector<Entity> World::CreateEntities(std::size_t num) requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    auto id = get_archetype_id<Components...>();
    if (m_Archetypes.count(id) == 0) {
        m_Archetypes.emplace(id, Archetype<Components...>::Create());
    }

    std::shared_ptr<IArchetype> archetype = m_Archetypes.at(id);

    std::pmr::vector<Entity> result;
    result.reserve(num);

    m_EnitiesMap.reserve(m_EnitiesMap.size() + num);
    for (std::size_t i = 0; i < num; i++) {
        auto&& [iter, success] = m_EnitiesMap.emplace(Entity{.id = m_Counter++}, archetype);
        assert(success);
        result.emplace_back(iter->first);
    }
    archetype->CreateInstances(result);

    return result;
}

template <typename Component>
std::optional<std::reference_wrapper<Component>> World::AccessEntity(const Entity& entity) requires utils::NoCVRef<Component> {
    auto iter = std::lower_bound(m_EnitiesMap.begin(), m_EnitiesMap.end(), entity.id, [](const auto& pair, std::uint64_t id) {
        return pair.first.id < id;
    });
    if (iter == m_EnitiesMap.end() || iter->first.id != entity.id) {
        return std::nullopt;
    }
    return iter->second->template GetComponent<Component>(entity);
}

template <typename... Components>
std::vector<std::shared_ptr<IArchetype>> World::GetArchetypes() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    std::vector<std::shared_ptr<IArchetype>> result;
    for (auto& [id, archetype] : m_Archetypes) {
        auto has_components = (true && ... && archetype->HasComponents<Components>());
        if (has_components)
            result.emplace_back(archetype);
    }
    return result;
}

template <typename Component>
bool World::HasEntity(const Entity& entity) requires utils::NoCVRef<Component> {
    return std::binary_search(m_EnitiesMap.begin(), m_EnitiesMap.end(), entity.id, [](const auto& pair, std::uint64_t id) {
        return pair.first.id == id;
    });
}

}  // namespace hitagi::ecs