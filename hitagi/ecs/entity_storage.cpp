module;
#include <cassert>

export module ecs:entity_storage;
import interop.spdlog;

import std;
import utils;

import :component;
import :archetype;
import :filter;

namespace hitagi::ecs::detail {

// Owns component storage and locations, without knowing Entity handles or World.
class EntityStorage {
public:
    explicit EntityStorage(std::shared_ptr<spdlog::logger> logger) : m_Logger(std::move(logger)) {}
    EntityStorage(const EntityStorage&)            = delete;
    EntityStorage& operator=(const EntityStorage&) = delete;

    void RegisterDynamicComponent(ComponentInfo component);
    auto GetDynamicComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo&;
    bool Has(entity_id_t entity) const noexcept { return m_EntityMaps.contains(entity); }
    auto NumEntities() const noexcept { return m_EntityMaps.size(); }
    // IDs are allocated consecutively; return the first without an extra ID buffer.
    auto CreateMany(std::size_t num, const ComponentInfoSet& component_infos) -> entity_id_t;
    void Destroy(entity_id_t entity);

    template <Component T>
    bool HasComponent(entity_id_t entity) const noexcept;
    bool HasDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const;

    template <Component T, typename... Args>
    auto EmplaceComponent(entity_id_t entity, Args&&... args) noexcept -> T&;

    auto AddDynamicComponent(entity_id_t entity, std::string_view dynamic_component) -> std::byte*;

    template <Component T>
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

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    std::size_t                     m_Counter = 0;

    std::pmr::unordered_map<archetype_id_t, std::unique_ptr<Archetype>> m_Archetypes;
    std::pmr::unordered_map<entity_id_t, Archetype*>                    m_EntityMaps;
    std::pmr::unordered_map<utils::TypeID, ComponentInfo>               m_ComponentMap;
};

template <Component T>
bool EntityStorage::HasComponent(entity_id_t entity) const noexcept {
    const auto& component_infos = m_EntityMaps.at(entity)->GetComponentInfoSet();
    return std::find_if(component_infos.begin(), component_infos.end(), [](const auto& info) { return info.type_id == utils::TypeID::Create<T>(); }) != component_infos.end();
}

template <Component T, typename... Args>
auto EntityStorage::EmplaceComponent(entity_id_t entity, Args&&... args) noexcept -> T& {
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
void EntityStorage::RemoveComponent(entity_id_t entity) noexcept {
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
auto EntityStorage::GetComponent(entity_id_t entity) const -> T& {
    if (!HasComponent<T>(entity)) {
        const auto error_message = std::format("Entity({}) does not have the component({})", entity, detail::create_static_component_info<T>().name);
        throw std::invalid_argument(error_message);
    }

    return m_EntityMaps.at(entity)->GetComponent<T>(entity);
}

template <Component T>
void EntityStorage::UpdateComponentInfo() noexcept {
    m_ComponentMap.emplace(utils::TypeID::Create<T>(), detail::create_static_component_info<T>());
}

template <Component T>
auto EntityStorage::GetComponentInfo() const noexcept -> const ComponentInfo& {
    return GetComponentInfo(utils::TypeID::Create<T>());
}

}  // namespace hitagi::ecs::detail

namespace hitagi::ecs::detail {

inline auto calculate_archetype_id(const detail::ComponentIdSet& component_ids) noexcept -> archetype_id_t {
    auto type_ids = component_ids                                                                            //
                    | std::ranges::views::transform([](const auto& type_id) { return type_id.GetValue(); })  //
                    | std::ranges::to<std::pmr::vector<std::size_t>>();
    std::sort(type_ids.begin(), type_ids.end());
    return utils::combine_hash(type_ids);
}

inline auto get_component_ids(const detail::ComponentInfoSet& component_infos) noexcept -> detail::ComponentIdSet {
    return component_infos                                                                                     //
           | std::ranges::views::transform([](const auto& component_info) { return component_info.type_id; })  //
           | std::ranges::to<detail::ComponentIdSet>();
}

void EntityStorage::RegisterDynamicComponent(ComponentInfo component) {
    component.type_id = utils::TypeID(component.name);
    m_ComponentMap.emplace(component.type_id, std::move(component));
}

auto EntityStorage::GetDynamicComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo& {
    const auto component_id = utils::TypeID(dynamic_component);
    if (!m_ComponentMap.contains(component_id)) {
        const auto error_message = std::format("Component {} not registered", dynamic_component);
        m_Logger->error(error_message);
        throw std::invalid_argument(error_message);
    }

    return GetComponentInfo(component_id);
}

auto EntityStorage::CreateMany(std::size_t num, const detail::ComponentInfoSet& component_infos) -> entity_id_t {
    const entity_id_t first     = m_Counter;
    auto&             archetype = GetOrCreateArchetype(component_infos);
    for (std::size_t index = 0; index < num; ++index) {
        const entity_id_t entity = m_Counter++;
        archetype.AllocateFor(entity);
        m_EntityMaps.emplace(entity, &archetype);
        for (const auto& component_info : component_infos) {
            archetype.DefaultConstructComponent(component_info.type_id, entity);
        }
    }
    return first;
}

void EntityStorage::Destroy(entity_id_t entity) {
    auto& archetype = *m_EntityMaps.at(entity);
    archetype.DestructAllComponents(entity);
    archetype.DeallocateFor(entity);
    m_EntityMaps.erase(entity);
}

bool EntityStorage::HasDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const {
    return m_EntityMaps.at(entity)->HasComponent(GetDynamicComponentInfo(dynamic_component).type_id);
}

auto EntityStorage::AddDynamicComponent(entity_id_t entity, std::string_view dynamic_component) -> std::byte* {
    if (HasDynamicComponent(entity, dynamic_component)) {
        return GetDynamicComponent(entity, dynamic_component);
    }

    const auto& dynamic_component_info = GetDynamicComponentInfo(dynamic_component);

    auto& old_archetype   = *m_EntityMaps.at(entity);
    auto  component_infos = old_archetype.GetComponentInfoSet();

    component_infos.emplace(dynamic_component_info);
    Archetype& new_archetype = GetOrCreateArchetype(component_infos);

    new_archetype.AllocateFor(entity);
    for (const auto& component_info : component_infos) {
        const auto component_id = component_info.type_id;
        if (component_id == dynamic_component_info.type_id) {
            new_archetype.DefaultConstructComponent(component_id, entity);
        } else {
            new_archetype.MoveConstructComponent(component_id, entity, old_archetype.GetComponentData(component_id, entity));
            old_archetype.DestructComponent(component_id, entity);
        }
    }

    old_archetype.DeallocateFor(entity);
    m_EntityMaps[entity] = &new_archetype;

    return GetDynamicComponent(entity, dynamic_component);
}

void EntityStorage::RemoveDynamicComponent(entity_id_t entity, std::string_view dynamic_component) {
    if (!HasDynamicComponent(entity, dynamic_component)) return;

    auto& old_archetype   = *m_EntityMaps.at(entity);
    auto  component_infos = old_archetype.GetComponentInfoSet();

    const auto removed_component_id = GetDynamicComponentInfo(dynamic_component).type_id;
    std::erase_if(component_infos, [=](const auto& info) { return info.type_id == removed_component_id; });
    Archetype& new_archetype = GetOrCreateArchetype(component_infos);

    new_archetype.AllocateFor(entity);

    for (const auto& component_info : component_infos) {
        const auto component_id = component_info.type_id;
        new_archetype.MoveConstructComponent(component_id, entity, old_archetype.GetComponentData(component_id, entity));
        old_archetype.DestructComponent(component_id, entity);
    }
    old_archetype.DestructComponent(removed_component_id, entity);

    old_archetype.DeallocateFor(entity);

    m_EntityMaps[entity] = &new_archetype;
}

auto EntityStorage::GetDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const -> std::byte* {
    const auto component_id = GetDynamicComponentInfo(dynamic_component).type_id;
    return m_EntityMaps.at(entity)->GetComponentData(component_id, entity);
}

auto EntityStorage::GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo& {
    return m_ComponentMap.at(component_id);
}

auto EntityStorage::GetOrCreateArchetype(const detail::ComponentInfoSet& component_infos) noexcept -> Archetype& {
    const auto archetype_id = calculate_archetype_id(get_component_ids(component_infos));
    if (!m_Archetypes.contains(archetype_id)) {
        m_Archetypes.emplace(archetype_id, std::make_unique<Archetype>(component_infos));
    }
    return *m_Archetypes[archetype_id];
}

auto EntityStorage::GetComponentsBuffers(const detail::ComponentIdList& components, Filter filter) const noexcept
    -> std::pmr::vector<std::pmr::vector<ComponentData>>

{
    auto new_filter = [&](Archetype* p_archetype) {
        for (const auto component_id : components) {
            if (!p_archetype->HasComponent(component_id)) return false;
        }

        if (filter && !filter(ComponentChecker(p_archetype)))
            return false;

        return true;
    };

    auto archetypes = m_Archetypes                                                                          //
                      | std::ranges::views::values                                                          //
                      | std::ranges::views::transform([](auto& p_archetype) { return p_archetype.get(); })  //
                      | std::ranges::views::filter(new_filter)                                              //
                      | std::ranges::to<std::pmr::vector<Archetype*>>();

    if (archetypes.empty()) return {};

    // [num_components, num_buffers]
    std::pmr::vector<std::pmr::vector<ComponentData>> result;

    for (const auto& component_id : components) {
        const auto& component_info = GetComponentInfo(component_id);

        auto component_data = archetypes  //
                              | std::ranges::views::transform([&component_info](auto p_archetype) {
                                    return p_archetype->GetComponentBuffers(component_info.type_id);
                                })                        //
                              | std::ranges::views::join  //
                              | std::ranges::views::transform([&component_info](auto& component_buffer) {
                                    return ComponentData{
                                        .data         = component_buffer.first,
                                        .size         = component_info.size,
                                        .num_entities = component_buffer.second,
                                    };
                                })  //
                              | std::ranges::to<std::pmr::vector<ComponentData>>();

        result.emplace_back(std::move(component_data));
    }

    const auto num_buffers = result.front().size();
    assert(std::ranges::all_of(result, [&](const auto& component_data) { return component_data.size() == num_buffers; }));
    for (std::size_t buffer_index = 0; buffer_index < num_buffers; ++buffer_index) {
        const auto num_entities = result.front()[buffer_index].num_entities;
        for (const auto& component_data : result) {
            assert(component_data[buffer_index].num_entities == num_entities);
        }
    }

    return result;
}

}  // namespace hitagi::ecs::detail
