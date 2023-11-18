#pragma once
#include <hitagi/ecs/component.hpp>
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/filter.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/types.hpp>

#include <memory>

namespace hitagi::ecs {
class World;
class Schedule;
class Archetype;

using ArchetypeID = std::size_t;

class EntityManager {
public:
    ~EntityManager();

    template <Component... Components>
    auto Create(const DynamicComponents& dynamic_components = {}) -> Entity
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>;

    template <Component... Components>
    auto CreateMany(std::size_t num, const DynamicComponents& dynamic_components = {}) noexcept -> std::pmr::vector<Entity>
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>;

    template <Component... Components>
    void Attach(Entity entity, const DynamicComponents& dynamic_components = {})
        requires utils::unique_types<Components...>;

    template <Component... Components>
    void Detach(Entity entity, const DynamicComponents& dynamic_components = {})
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>;

    void Destroy(Entity entity);

    inline bool Has(Entity entity) const noexcept { return m_EntityMaps.contains(entity); }

    inline auto NumEntities() const noexcept { return m_EntityMaps.size(); }

    template <Component T>
    auto GetComponent(Entity entity) noexcept -> utils::optional_ref<T>;

    template <Component T>
    auto GetComponent(Entity entity) const noexcept -> utils::optional_ref<const T>;

    auto GetDynamicComponent(Entity entity, const DynamicComponent& dynamic_component) const noexcept -> std::byte*;

private:
    friend World;
    friend Schedule;

    EntityManager(World& world);

    auto CreateMany(std::size_t num, const detail::ComponentInfos& component_infos) noexcept -> std::pmr::vector<Entity>;
    void Attach(Entity entity, const detail::ComponentInfos& component_infos);
    void Detach(Entity entity, const detail::ComponentInfos& component_infos);
    auto GetComponent(Entity entity, const detail::ComponentInfo& component_info) const noexcept -> std::byte*;
    auto GetOrCreateArchetype(const detail::ComponentInfos& component_infos) noexcept -> Archetype&;

    using ComponentBuffer = std::pair<std::byte*, std::size_t>;
    auto GetComponentsBuffers(const detail::ComponentInfos& component_infos, Filter filter) const noexcept
        -> std::pmr::vector<std::pmr::vector<ComponentBuffer>>;  // [num_buffers, num_components]

    World& m_World;

    std::size_t m_Counter = 0;

    std::pmr::unordered_map<ArchetypeID, std::unique_ptr<Archetype>> m_Archetypes;
    std::pmr::unordered_map<Entity, Archetype*>                      m_EntityMaps;
};

template <Component... Components>
auto EntityManager::Create(const DynamicComponents& dynamic_components) -> Entity
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
{
    return CreateMany<Components...>(1, dynamic_components)[0];
}

template <Component... Components>
auto EntityManager::CreateMany(std::size_t num, const DynamicComponents& dynamic_components) noexcept -> std::pmr::vector<Entity>
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
{
    return CreateMany(num, detail::create_component_infos<Entity, Components...>(dynamic_components));
}

template <Component... Components>
void EntityManager::Attach(Entity entity, const DynamicComponents& dynamic_components)
    requires utils::unique_types<Components...>
{
    Attach(entity, detail::create_component_infos<Components...>(dynamic_components));
}

template <Component... Components>
void EntityManager::Detach(Entity entity, const DynamicComponents& dynamic_components)
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
{
    Detach(entity, detail::create_component_infos<Components...>(dynamic_components));
}

template <Component T>
auto EntityManager::GetComponent(Entity entity) noexcept -> utils::optional_ref<T> {
    auto ptr = GetComponent(entity, detail::create_component_infos<T>().front());
    if (ptr == nullptr) return std::nullopt;
    return *reinterpret_cast<T*>(ptr);
}

template <Component T>
auto EntityManager::GetComponent(Entity entity) const noexcept -> utils::optional_ref<const T> {
    auto ptr = GetComponent(entity, detail::create_component_infos<T>().front());
    if (ptr == nullptr) return std::nullopt;
    return *reinterpret_cast<const T*>(ptr);
}

}  // namespace hitagi::ecs