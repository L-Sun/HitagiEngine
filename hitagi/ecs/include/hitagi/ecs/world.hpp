#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/archetype.hpp>
#include <hitagi/core/timer.hpp>

#include <algorithm>
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
    { T::OnUpdate(s, delta) } -> std::same_as<void>;
};

class World {
public:
    World(std::string_view name);

    void Update();

    template <SystemLike System>
    void RegisterSystem(std::string_view name);

    inline std::size_t NumEntities() const noexcept { return m_EnitiesMap.size(); }

    // create an entity that has multiple order independent components.
    template <typename... Components>
    Entity CreateEntity() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    // create an entity that has multiple order independent components.
    template <typename... Components>
    Entity CreateEntity(Components&&... components) requires utils::UniqueTypes<std::remove_cvref_t<Components>...>;

    template <typename... Components>
    std::pmr::vector<Entity> CreateEntities(std::size_t num) requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    void DestoryEntity(const Entity& entity);
    // Get a array of archetype which contains the interseted components
    template <typename... Components>
    std::pmr::vector<std::shared_ptr<IArchetype>> GetArchetypes() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);
    template <typename... Components>
    std::pmr::vector<std::shared_ptr<IArchetype>> GetArchetypes() const requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    template <typename... Components>
    std::pmr::vector<ecs::Entity> GetEntities() const requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...);

    template <typename Component>
    std::optional<std::reference_wrapper<Component>> GetComponent(const Entity& entity) requires utils::NoCVRef<Component>;

    template <typename Component>
    std::optional<std::reference_wrapper<const Component>> GetComponent(const Entity& entity) const requires utils::NoCVRef<Component>;

    template <typename Component>
    bool HasEntity(const Entity& entity) requires utils::NoCVRef<Component>;

    bool stop = false;

private:
    void LogMessage(std::string_view message);

    std::pmr::string                                                  m_Name;
    std::shared_ptr<spdlog::logger>                                   m_Logger;
    core::Clock                                                       m_Timer;
    std::size_t                                                       m_Counter = 0;
    std::pmr::unordered_map<Entity, std::shared_ptr<IArchetype>>      m_EnitiesMap;
    std::pmr::unordered_map<ArchetypeId, std::shared_ptr<IArchetype>> m_Archetypes;

    std::pmr::unordered_map<std::pmr::string, std::function<void(Schedule&, std::chrono::duration<double>)>> m_Systems;
};

template <SystemLike System>
void World::RegisterSystem(std::string_view name) {
    std::pmr::string _name{name};
    if (m_Systems.contains(std::pmr::string(_name))) {
        LogMessage(fmt::format("The system ({}) exsisted!", name));
    } else
        m_Systems.emplace(_name, &System::OnUpdate);
}

template <typename... Components>
Entity World::CreateEntity() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    auto id = get_archetype_id<Components...>();
    if (!m_Archetypes.contains(id)) {
        m_Archetypes.emplace(id, Archetype<Components...>::Create());
    }

    std::shared_ptr<IArchetype> archetype = m_Archetypes.at(id);

    auto&& [iter, success] = m_EnitiesMap.emplace(Entity{.id = m_Counter++}, archetype);
    assert(success);
    archetype->CreateInstances({iter->first});

    return iter->first;
}

// create an entity that has multiple order independent components.
template <typename... Components>
Entity World::CreateEntity(Components&&... components) requires utils::UniqueTypes<std::remove_cvref_t<Components>...> {
    auto entity = CreateEntity<std::remove_cvref_t<Components>...>();
    ((GetComponent<std::remove_cvref_t<Components>>(entity)->get() = std::forward<Components>(components)), ...);
    return entity;
}

template <typename... Components>
std::pmr::vector<Entity> World::CreateEntities(std::size_t num) requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    auto id = get_archetype_id<Components...>();
    if (!m_Archetypes.contains(id)) {
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
std::optional<std::reference_wrapper<Component>> World::GetComponent(const Entity& entity) requires utils::NoCVRef<Component> {
    auto result = const_cast<const World*>(this)->GetComponent<Component>(entity);
    if (result.has_value()) {
        return const_cast<Component&>(result->get());
    } else {
        return std::nullopt;
    }
}

template <typename Component>
std::optional<std::reference_wrapper<const Component>> World::GetComponent(const Entity& entity) const requires utils::NoCVRef<Component> {
    if (!m_EnitiesMap.contains(entity)) return std::nullopt;
    return m_EnitiesMap.at(entity)->GetComponent<Component>(entity);
}

template <typename... Components>
std::pmr::vector<std::shared_ptr<IArchetype>> World::GetArchetypes() const requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    std::pmr::vector<std::shared_ptr<IArchetype>> result;
    for (auto& [id, archetype] : m_Archetypes) {
        auto has_components = (true && ... && archetype->HasComponents<Components>());
        if (has_components)
            result.emplace_back(archetype);
    }
    return result;
}

template <typename... Components>
std::pmr::vector<std::shared_ptr<IArchetype>> World::GetArchetypes() requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    return const_cast<const World*>(this)->GetArchetypes<Components...>();
}

template <typename Component>
bool World::HasEntity(const Entity& entity) requires utils::NoCVRef<Component> {
    return std::binary_search(m_EnitiesMap.begin(), m_EnitiesMap.end(), entity.id, [](const auto& pair, std::uint64_t id) {
        return pair.first.id == id;
    });
}

template <typename... Components>
std::pmr::vector<ecs::Entity> World::GetEntities() const requires utils::UniqueTypes<Components...> &&(utils::NoCVRef<Components>&&...) {
    std::pmr::vector<ecs::Entity> result;
    for (const auto& archetype : GetArchetypes<Components...>()) {
        auto entities = archetype->GetAllEntities();
        result.insert(result.end(), std::make_move_iterator(entities.begin()), std::make_move_iterator(entities.end()));
    }
    return result;
}

}  // namespace hitagi::ecs