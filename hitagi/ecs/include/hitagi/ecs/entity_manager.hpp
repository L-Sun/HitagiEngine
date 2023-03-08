#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/archetype.hpp>
#include <hitagi/ecs/filter.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/types.hpp>

namespace hitagi::ecs {
class World;

class EntityManager {
public:
    template <Component... Components>
    auto Create(const DynamicComponents& dynamic_components = {}) -> Entity
        requires utils::UniqueTypes<Components...>;

    template <Component... Components>
    auto CreateMany(std::size_t num, const DynamicComponents& dynamic_components = {}) -> std::pmr::vector<Entity>
        requires utils::UniqueTypes<Components...>;

    template <Component... Components>
    void Attach(Entity entity, const DynamicComponents& dynamic_components = {})
        requires utils::UniqueTypes<Components...>;

    template <Component... Components>
    void Detach(Entity entity, const DynamicComponents& dynamic_components = {})
        requires utils::UniqueTypes<Components...>;

    void Destory(Entity entity) noexcept;

    inline bool Has(Entity entity) const noexcept { return m_EntityMaps.contains(entity); }

    inline auto NumEntities() const noexcept { return m_EntityMaps.size(); }

    template <Component T>
    auto GetComponent(Entity entity) -> std::optional<std::reference_wrapper<T>>;

    template <Component T>
    auto GetComponent(Entity entity) const -> std::optional<std::reference_wrapper<const T>>;

    auto GetArchetype(const Filter& filter) const -> std::pmr::vector<detials::IArchetype*>;

private:
    friend class World;

    EntityManager(World& world) : m_World(world) {}

    World& m_World;

    std::size_t m_Counter = 0;

    std::pmr::unordered_map<detials::ArchetypeID, std::shared_ptr<detials::IArchetype>> m_Archetypes;
    std::pmr::unordered_map<Entity, detials::IArchetype*>                               m_EntityMaps;
};

template <Component... Components>
auto EntityManager::Create(const DynamicComponents& dynamic_components) -> Entity
    requires utils::UniqueTypes<Components...>
{
    return CreateMany<Components...>(1, dynamic_components)[0];
}

template <Component... Components>
auto EntityManager::CreateMany(std::size_t num, const DynamicComponents& dynamic_components) -> std::pmr::vector<Entity>
    requires utils::UniqueTypes<Components...>
{
    auto archetype_id = detials::get_archetype_id<Components...>(dynamic_components);
    // no component
    if (archetype_id == 0)
        return {};

    if (!m_Archetypes.contains(archetype_id)) {
        m_Archetypes.emplace(archetype_id, std::make_shared<detials::Archetype<Components...>>(dynamic_components));
    }

    detials::IArchetype* archetype = m_Archetypes.at(archetype_id).get();

    std::pmr::vector<Entity> result(num);
    for (auto& entity : result) {
        entity.id = m_Counter++;
        m_EntityMaps.emplace(entity, archetype);
    }

    archetype->CreateEntities(result);

    return result;
}

// TODO
template <Component... Components>
void EntityManager::Attach(Entity entity, const DynamicComponents& dynamic_components)
    requires utils::UniqueTypes<Components...>
{
    if (!m_EntityMaps.contains(entity))
        return;

    auto old_archetype = m_EntityMaps[entity];
    if (old_archetype->ID() == detials::get_archetype_id<Components...>(dynamic_components))
        return;
}

template <Component... Components>
void EntityManager::Detach(Entity entity, const DynamicComponents& dynamic_components)
    requires utils::UniqueTypes<Components...>
{}

template <Component T>
auto EntityManager::GetComponent(Entity entity) -> std::optional<std::reference_wrapper<T>> {
    if (!m_EntityMaps.contains(entity)) return std::nullopt;

    return m_EntityMaps[entity]->GetComponent<T>(entity);
}

template <Component T>
auto EntityManager::GetComponent(Entity entity) const -> std::optional<std::reference_wrapper<const T>> {
    if (!m_EntityMaps.contains(entity)) return std::nullopt;

    return m_EntityMaps.at(entity)->GetComponent<const T>(entity);
}

}  // namespace hitagi::ecs