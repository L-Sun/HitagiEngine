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

    void RegisterDynamicComponent(ComponentInfo dynamic_component);

    template <Component T>
    auto GetComponentInfo() const -> const ComponentInfo&;
    auto GetComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo&;

    template <Component... Components>
    auto Create(const DynamicComponentSet& dynamic_components = {}) -> Entity
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>;

    template <Component... Components>
    auto CreateMany(std::size_t num, const DynamicComponentSet& dynamic_components = {}) noexcept -> std::pmr::vector<Entity>
        requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>;

    void Destroy(Entity& entity);

    auto NumEntities() const noexcept { return m_EntityMaps.size(); }

private:
    friend World;
    friend Schedule;
    friend Entity;

    EntityManager(World& world);

    void UpdateComponentInfos(const detail::ComponentInfoSet& component_infos);
    auto GetComponentInfo(utils::TypeID component_id) const -> const ComponentInfo&;
    auto CreateMany(std::size_t num, const detail::ComponentInfoSet& component_infos) noexcept -> std::pmr::vector<Entity>;
    void Attach(Entity& entity, const detail::ComponentInfoSet& component_infos);
    void Detach(Entity& entity, const detail::ComponentInfoSet& component_infos);
    auto GetOrCreateArchetype(const detail::ComponentInfoSet& component_infos) -> Archetype&;

    struct ComponentData {
        std::byte*  data;
        std::size_t size;
        std::size_t num_entities;

        auto operator[](std::size_t index) const noexcept -> std::byte* { return data + index * size; }
    };
    auto GetComponentsBuffers(const detail::ComponentIdList& components, Filter filter) const noexcept
        -> std::pmr::vector<std::pmr::vector<ComponentData>>;  // [num_components, num_buffers]

    World& m_World;

    std::size_t m_Counter = 0;

    std::pmr::unordered_map<ArchetypeID, std::unique_ptr<Archetype>> m_Archetypes;
    std::pmr::unordered_map<Entity, Archetype*>                      m_EntityMaps;
    std::pmr::unordered_map<utils::TypeID, ComponentInfo>            m_ComponentMap;
};

template <Component T>
auto EntityManager::GetComponentInfo() const -> const ComponentInfo& {
    return GetComponentInfo(utils::TypeID::Create<T>());
}

template <Component... Components>
auto EntityManager::Create(const DynamicComponentSet& dynamic_components) -> Entity
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
{
    return CreateMany<Components...>(1, dynamic_components)[0];
}

template <Component... Components>
auto EntityManager::CreateMany(std::size_t num, const DynamicComponentSet& dynamic_components) noexcept -> std::pmr::vector<Entity>
    requires utils::unique_types<Components...> && utils::no_in<Entity, Components...>
{
    detail::ComponentInfoSet dynamic_component_infos;
    for (const auto& component : dynamic_components) {
        dynamic_component_infos.emplace(GetComponentInfo(component));
    }
    return CreateMany(num, detail::create_component_info_set<Entity, Components...>(dynamic_component_infos));
}

}  // namespace hitagi::ecs