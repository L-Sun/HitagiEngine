#include "archetype.hpp"

#include <hitagi/ecs/component.hpp>
#include <hitagi/ecs/entity_manager.hpp>
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

inline auto calculate_archetype_id(const detail::ComponentIdSet& component_ids) noexcept -> ArchetypeID {
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

EntityManager::EntityManager(World& world) : m_World(world) {}
EntityManager::~EntityManager() {}

void EntityManager::RegisterDynamicComponent(ComponentInfo component) {
    component.type_id = utils::TypeID(component.name);
    m_ComponentMap.emplace(component.type_id, std::move(component));
}

auto EntityManager::GetComponentInfo(std::string_view dynamic_component) const -> const ComponentInfo& {
    return GetComponentInfo(utils::TypeID(dynamic_component));
}

void EntityManager::UpdateComponentInfos(const detail::ComponentInfoSet& component_infos) {
    for (const auto& component_info : component_infos) {
        if (!m_ComponentMap.contains(component_info.type_id)) {
            m_ComponentMap.emplace(component_info.type_id, component_info);
        }
    }
}

auto EntityManager::GetComponentInfo(utils::TypeID component_id) const -> const ComponentInfo& {
    return m_ComponentMap.at(component_id);
}

auto EntityManager::CreateMany(std::size_t num, const detail::ComponentInfoSet& component_infos) noexcept -> std::pmr::vector<Entity> {
    UpdateComponentInfos(component_infos);

    auto& archetype = GetOrCreateArchetype(component_infos);

    auto entities = ranges::views::iota(m_Counter, m_Counter + num)                        //
                    | ranges::views::transform([&](auto id) { return Entity{this, id}; })  //
                    | ranges::to<std::pmr::vector<Entity>>();

    m_Counter += num;

    archetype.CreateEntities(entities);

    for (const auto entity : entities) {
        m_EntityMaps.emplace(entity, &archetype);
    }

    return entities;
}

void EntityManager::Attach(Entity& entity, const detail::ComponentInfoSet& component_infos) {
    if (component_infos.empty()) return;

    UpdateComponentInfos(component_infos);

    auto& old_archetype = *m_EntityMaps.at(entity);

    auto new_component_infos = component_infos;
    for (const auto& new_component_info : new_component_infos) {
        if (entity.HasComponent(new_component_info.type_id)) {
            const auto error_message = fmt::format(
                "Entity {} already has component {}",
                entity.m_Id,
                new_component_info.type_id.GetValue());
            m_World.GetLogger()->error(error_message);
            throw std::invalid_argument(error_message);
        }
    }
    auto old_component_infos = old_archetype.GetComponentInfoSet();
    new_component_infos.merge(old_component_infos);

    auto& new_archetype = GetOrCreateArchetype(new_component_infos);
    new_archetype.CreateEntities({entity});

    for (const auto& old_component_info : old_archetype.GetComponentInfoSet()) {
        auto old_component_ptr = old_archetype.GetComponentData(entity, old_component_info.type_id);
        auto new_component_ptr = new_archetype.GetComponentData(entity, old_component_info.type_id);
        std::memcpy(new_component_ptr, old_component_ptr, old_component_info.size);
    }

    old_archetype.DeleteEntity(entity);
    m_EntityMaps.at(entity) = &new_archetype;
}

void EntityManager::Detach(Entity& entity, const detail::ComponentInfoSet& component_infos) {
    if (component_infos.empty()) return;

    auto& old_archetype = *m_EntityMaps.at(entity);

    auto new_component_infos = old_archetype.GetComponentInfoSet();
    for (const auto& component_info : component_infos) {
        if (!entity.HasComponent(component_info.type_id)) {
            const auto error_message = fmt::format(
                "Entity {} does not have component {}",
                entity.m_Id,
                component_info.type_id.GetValue());
            m_World.GetLogger()->error(error_message);
            throw std::invalid_argument(error_message);
        }
        std::erase_if(new_component_infos, [&](const auto& info) { return info == component_info; });
    }

    auto& new_archetype = GetOrCreateArchetype(new_component_infos);
    new_archetype.CreateEntities({entity});

    for (const auto& new_component_info : new_component_infos) {
        auto old_component_ptr = old_archetype.GetComponentData(entity, new_component_info.type_id);
        auto new_component_ptr = new_archetype.GetComponentData(entity, new_component_info.type_id);
        std::memcpy(new_component_ptr, old_component_ptr, new_component_info.size);
    }

    old_archetype.DeleteEntity(entity);
    m_EntityMaps.at(entity) = &new_archetype;
}

void EntityManager::Destroy(Entity& entity) {
    m_EntityMaps.at(entity)->DeleteEntity(entity);
    m_EntityMaps.erase(entity);
    entity = {};
}

auto EntityManager::GetOrCreateArchetype(const detail::ComponentInfoSet& component_infos) -> Archetype& {
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
        return (filter ? filter(ComponentChecker(p_archetype)) : true) &&
               ranges::all_of(components, [&](const auto component_id) {
                   return p_archetype->HasComponent(component_id);
               });
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
        const auto& component_info = m_ComponentMap.at(component_id);

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