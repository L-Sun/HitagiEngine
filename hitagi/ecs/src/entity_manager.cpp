#include <hitagi/ecs/entity_manager.hpp>
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/component.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/utils/hash.hpp>

#include <fmt/color.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/logger.h>

namespace hitagi::ecs {

inline auto calculate_archetype_id(const detail::ComponentIdSet& component_ids) noexcept -> archetype_id_t {
    auto type_ids = component_ids                                                                       //
                    | ranges::views::transform([](const auto& type_id) { return type_id.GetValue(); })  //
                    | ranges::to<std::pmr::vector<std::size_t>>();
    std::sort(type_ids.begin(), type_ids.end());
    return utils::combine_hash(type_ids);
}

inline auto get_component_ids(const detail::ComponentInfoSet& component_infos) noexcept -> detail::ComponentIdSet {
    return component_infos                                                                                //
           | ranges::views::transform([](const auto& component_info) { return component_info.type_id; })  //
           | ranges::to<detail::ComponentIdSet>();
}

EntityManager::EntityManager(World& world) : m_World(world) {
    UpdateComponentInfo<Entity>();
}
EntityManager::~EntityManager() = default;  // forward declaration of unique_ptr<Archetype>

void EntityManager::RegisterDynamicComponent(ComponentInfo component) {
    component.type_id = utils::TypeID(component.name);
    m_ComponentMap.emplace(component.type_id, std::move(component));
}

auto EntityManager::GetDynamicComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo& {
    const auto component_id = utils::TypeID(dynamic_component);
    if (!m_ComponentMap.contains(component_id)) {
        const auto error_message = fmt::format("Component {} not registered", dynamic_component);
        m_World.GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    return GetComponentInfo(component_id);
}

bool EntityManager::Has(Entity entity) const noexcept {
    return m_EntityMaps.contains(entity.GetId());
}

auto EntityManager::Create() noexcept -> Entity {
    return CreateMany(1).front();
}

auto EntityManager::CreateMany(std::size_t num, const detail::ComponentInfoSet& component_infos) noexcept -> std::pmr::vector<Entity> {
    std::pmr::vector<Entity> entities;
    entities.reserve(num);

    auto& archetype = GetOrCreateArchetype(component_infos);
    for (entity_id_t entity = m_Counter; entity < m_Counter + num; entity++) {
        archetype.AllocateFor(entity);
        m_EntityMaps.emplace(entity, &archetype);
    }

    for (const auto& component_info : component_infos) {
        if (component_info.type_id == utils::TypeID::Create<Entity>()) {
            for (entity_id_t entity = m_Counter; entity < m_Counter + num; entity++) {
                entities.emplace_back(archetype.ConstructComponent<Entity>(entity, Entity(this, entity)));
            }
        } else {
            for (entity_id_t entity = m_Counter; entity < m_Counter + num; entity++) {
                archetype.DefaultConstructComponent(component_info.type_id, entity);
            }
        }
    }

    m_Counter += num;

    return entities;
}

void EntityManager::Destroy(Entity& entity) {
    auto& archetype = *m_EntityMaps.at(entity.GetId());
    archetype.DestructAllComponents(entity.GetId());
    archetype.DeallocateFor(entity.GetId());
    m_EntityMaps.erase(entity.GetId());
    entity = {};
}

bool EntityManager::HasDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const {
    return m_EntityMaps.at(entity)->HasComponent(GetDynamicComponentInfo(dynamic_component).type_id);
}

auto EntityManager::AddDynamicComponent(entity_id_t entity, std::string_view dynamic_component) -> std::byte* {
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

    m_EntityMaps[entity] = &new_archetype;

    return GetDynamicComponent(entity, dynamic_component);
}

void EntityManager::RemoveDynamicComponent(entity_id_t entity, std::string_view dynamic_component) {
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

auto EntityManager::GetDynamicComponent(entity_id_t entity, std::string_view dynamic_component) const -> std::byte* {
    const auto component_id = GetDynamicComponentInfo(dynamic_component).type_id;
    return m_EntityMaps.at(entity)->GetComponentData(component_id, entity);
}

auto EntityManager::GetComponentInfo(utils::TypeID component_id) const noexcept -> const ComponentInfo& {
    return m_ComponentMap.at(component_id);
}

auto EntityManager::GetOrCreateArchetype(const detail::ComponentInfoSet& component_infos) noexcept -> Archetype& {
    const auto archetype_id = calculate_archetype_id(get_component_ids(component_infos));
    if (!m_Archetypes.contains(archetype_id)) {
        m_Archetypes.emplace(archetype_id, std::make_unique<Archetype>(component_infos));
    }
    return *m_Archetypes[archetype_id];
}

auto EntityManager::GetComponentsBuffers(const detail::ComponentIdList& components, Filter filter) const noexcept
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

    auto archetypes = m_Archetypes                                                                     //
                      | ranges::views::values                                                          //
                      | ranges::views::transform([](auto& p_archetype) { return p_archetype.get(); })  //
                      | ranges::views::filter(new_filter)                                              //
                      | ranges::to<std::pmr::vector<Archetype*>>();

    if (archetypes.empty()) return {};

    // [num_components, num_buffers]
    std::pmr::vector<std::pmr::vector<ComponentData>> result;

    for (const auto& component_id : components) {
        const auto& component_info = GetComponentInfo(component_id);

        auto component_data = archetypes  //
                              | ranges::views::transform([&component_info](auto p_archetype) {
                                    return p_archetype->GetComponentBuffers(component_info.type_id);
                                })                   //
                              | ranges::views::join  //
                              | ranges::views::transform([&component_info](auto& component_buffer) {
                                    return ComponentData{
                                        .data         = component_buffer.first,
                                        .size         = component_info.size,
                                        .num_entities = component_buffer.second,
                                    };
                                })  //
                              | ranges::to<std::pmr::vector<ComponentData>>();

        result.emplace_back(std::move(component_data));
    }

    const auto num_buffers = result.front().size();
    assert(ranges::all_of(result, [&](const auto& component_data) { return component_data.size() == num_buffers; }));
    for (const auto buffer_index : ranges::views::indices(num_buffers)) {
        const auto num_entities = result.front()[buffer_index].num_entities;
        for (const auto& component_data : result) {
            assert(component_data[buffer_index].num_entities == num_entities);
        }
    }

    return result;
}

}  // namespace hitagi::ecs