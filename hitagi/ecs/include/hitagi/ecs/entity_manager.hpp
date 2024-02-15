#pragma once
#include <hitagi/ecs/common_types.hpp>
#include <hitagi/ecs/archetype.hpp>
#include <hitagi/ecs/filter.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/types.hpp>

namespace hitagi::ecs {

class EntityManager {
public:
    ~EntityManager();

    void RegisterDynamicComponent(ComponentInfo dynamic_component);
    auto GetDynamicComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo&;

    bool Has(Entity entity) const noexcept;

    [[nodiscard]] auto Create() noexcept -> Entity;
    void               Destroy(Entity& entity);

    template <Component... Components>
        requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
    [[nodiscard]] auto CreateMany(std::size_t num, const std::pmr::set<std::string_view>& dynamic_components = {}) -> std::pmr::vector<Entity>;

    auto NumEntities() const noexcept { return m_EntityMaps.size(); }

private:
    friend World;
    friend Schedule;
    friend Entity;

    EntityManager(World& world);

    auto CreateMany(std::size_t num, const detail::ComponentInfoSet& component_infos) noexcept -> std::pmr::vector<Entity>;

    template <Component T>
    bool HasComponent(entity_id_t entity) const noexcept;
    bool HasDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const;

    template <Component T, typename... Args>
        requires utils::not_same_as<T, Entity>
    auto EmplaceComponent(entity_id_t entity, Args&&... args) noexcept -> T&;

    auto AddDynamicComponent(entity_id_t entity, std::string_view dynamic_component) -> std::byte*;

    template <Component T>
        requires utils::not_same_as<T, Entity>
    void RemoveComponent(entity_id_t entity) noexcept;
    void RemoveDynamicComponent(entity_id_t entity, std::string_view dynamic_component);

    template <Component T>
    auto GetComponent(entity_id_t entity) const -> T&;
    auto GetDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const -> std::byte*;

    template <Component T>
    void UpdateComponentInfo() noexcept;

    template <Component T>
    auto GetComponentInfo() const noexcept -> const ComponentInfo&;
    auto GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo&;

    auto GetOrCreateArchetype(const detail::ComponentInfoSet& component_infos) noexcept -> Archetype&;

    struct ComponentData {
        std::byte*  data;
        std::size_t size;
        std::size_t num_entities;

        auto operator[](std::size_t index) const noexcept -> std::byte* { return data + index * size; }
    };
    auto GetComponentsBuffers(const detail::ComponentIdList& components, Filter filter) const noexcept
        -> std::pmr::vector<std::pmr::vector<ComponentData>>;

    World& m_World;

    std::size_t m_Counter = 0;

    std::pmr::unordered_map<archetype_id_t, std::unique_ptr<Archetype>> m_Archetypes;
    std::pmr::unordered_map<entity_id_t, Archetype*>                    m_EntityMaps;
    std::pmr::unordered_map<utils::TypeID, ComponentInfo>               m_ComponentMap;
};

template <Component... Components>
    requires((std::default_initializable<Components> && utils::not_same_as<Components, Entity>) && ...)
[[nodiscard]] auto EntityManager::CreateMany(std::size_t num, const std::pmr::set<std::string_view>& dynamic_components) -> std::pmr::vector<Entity> {
    (UpdateComponentInfo<Components>(), ...);

    auto component_infos = detail::create_component_info_set<Entity, Components...>();
    for (auto dynamic_component : dynamic_components) {
        component_infos.emplace(GetDynamicComponentInfo(dynamic_component));
    }

    return CreateMany(num, component_infos);
}

template <Component T>
bool EntityManager::HasComponent(entity_id_t entity) const noexcept {
    const auto& component_infos = m_EntityMaps.at(entity)->GetComponentInfoSet();
    return std::find_if(component_infos.begin(), component_infos.end(), [](const auto& info) { return info.type_id == utils::TypeID::Create<T>(); }) != component_infos.end();
}

template <Component T, typename... Args>
    requires utils::not_same_as<T, Entity>
auto EntityManager::EmplaceComponent(entity_id_t entity, Args&&... args) noexcept -> T& {
    if (HasComponent<T>(entity)) return GetComponent<T>(entity);

    UpdateComponentInfo<T>();
    const auto new_component_id = utils::TypeID::Create<T>();

    auto& old_archetype   = *m_EntityMaps.at(entity);
    auto  component_infos = old_archetype.GetComponentInfoSet();

    component_infos.emplace(detail::create_static_component_info<T>());
    Archetype& new_archetype = GetOrCreateArchetype(component_infos);

    new_archetype.AllocateFor(entity);

    for (const auto& component_info : component_infos) {
        const auto component_id = component_info.type_id;
        if (new_component_id == component_id) {
            new_archetype.ConstructComponent<T>(entity, std::forward<Args>(args)...);
        } else {
            new_archetype.MoveConstructComponent(component_id, entity, old_archetype.GetComponentData(component_id, entity));
            old_archetype.DestructComponent(component_id, entity);
        }
    }

    old_archetype.DeallocateFor(entity);

    m_EntityMaps[entity] = &new_archetype;

    return new_archetype.GetComponent<T>(entity);
}

template <Component T>
    requires utils::not_same_as<T, Entity>
void EntityManager::RemoveComponent(entity_id_t entity) noexcept {
    if (!HasComponent<T>(entity)) return;

    auto& old_archetype   = *m_EntityMaps.at(entity);
    auto  component_infos = old_archetype.GetComponentInfoSet();

    const auto removed_component_id = utils::TypeID::Create<T>();
    std::erase_if(component_infos, [=](const auto& info) { return info.type_id == removed_component_id; });
    Archetype& new_archetype = GetOrCreateArchetype(component_infos);

    new_archetype.AllocateFor(entity);

    for (const auto& component_info : component_infos) {
        const auto component_id = component_info.type_id;
        new_archetype.MoveConstructComponent(component_id, entity, old_archetype.GetComponentData(component_id, entity));
        old_archetype.DestructComponent(component_id, entity);
    }
    old_archetype.DestructComponent<T>(entity);

    old_archetype.DeallocateFor(entity);

    m_EntityMaps[entity] = &new_archetype;
}

template <Component T>
auto EntityManager::GetComponent(entity_id_t entity) const -> T& {
    if (!HasComponent<T>(entity)) {
        const auto error_message = fmt::format("Entity({}) does not have the component({})", entity, detail::create_static_component_info<T>().name);
        throw std::invalid_argument(error_message);
    }

    return m_EntityMaps.at(entity)->GetComponent<T>(entity);
}

template <Component T>
void EntityManager::UpdateComponentInfo() noexcept {
    m_ComponentMap.emplace(utils::TypeID::Create<T>(), detail::create_static_component_info<T>());
}

template <Component T>
auto EntityManager::GetComponentInfo() const noexcept -> const ComponentInfo& {
    return GetComponentInfo(utils::TypeID::Create<T>());
}

}  // namespace hitagi::ecs