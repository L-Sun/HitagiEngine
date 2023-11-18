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
#include <range/v3/view/join.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/logger.h>

namespace hitagi::ecs {

inline auto calculate_archetype_id(const detail::ComponentInfos& component_infos) noexcept -> ArchetypeID {
    auto type_ids = component_infos                                                                       //
                    | ranges::views::transform([](const auto& info) { return info.type_id.GetValue(); })  //
                    | ranges::to<std::pmr::vector<std::size_t>>();
    std::sort(type_ids.begin(), type_ids.end());
    return utils::combine_hash(type_ids);
}

EntityManager::EntityManager(World& world) : m_World(world) {}
EntityManager::~EntityManager() {}

auto EntityManager::CreateMany(std::size_t num, const detail::ComponentInfos& component_infos) noexcept -> std::pmr::vector<Entity> {
    auto& archetype = GetOrCreateArchetype(component_infos);

    auto entities = ranges::views::iota(m_Counter, m_Counter + num)                       //
                    | ranges::views::transform([](auto id) { return Entity{.id = id}; })  //
                    | ranges::to<std::pmr::vector<Entity>>();

    m_Counter += num;

    archetype.CreateEntities(entities);

    for (const auto entity : entities) {
        m_EntityMaps.emplace(entity, &archetype);
    }

    return entities;
}

void EntityManager::Attach(Entity entity, const detail::ComponentInfos& component_infos) {
    if (component_infos.empty()) return;

    auto new_component_infos = component_infos;
    {
        auto old_component_infos = m_EntityMaps[entity]->GetComponentInfos();
        for (const auto& old_component_info : old_component_infos) {
            if (ranges::find(new_component_infos, old_component_info) != new_component_infos.end()) {
                const auto error_message = fmt::format(
                    "Entity {} already has component {}",
                    entity.id,
                    old_component_info.type_id.GetValue());
                m_World.m_Logger->error(error_message);
                throw std::invalid_argument(error_message);
            }
            new_component_infos.emplace_back(old_component_info);
        }
    }

    auto& new_archetype = GetOrCreateArchetype(new_component_infos);
    auto& old_archetype = *m_EntityMaps[entity];
    new_archetype.CreateEntities({entity});

    for (const auto& old_component_info : old_archetype.GetComponentInfos()) {
        auto old_component_ptr = old_archetype.GetComponentData(old_component_info, entity);
        auto new_component_ptr = new_archetype.GetComponentData(old_component_info, entity);
        std::memcpy(new_component_ptr, old_component_ptr, old_component_info.size);
    }

    old_archetype.DeleteEntity(entity);
    m_EntityMaps[entity] = &new_archetype;
}

void EntityManager::Detach(Entity entity, const detail::ComponentInfos& component_infos) {
    if (component_infos.empty()) return;

    auto new_component_infos = m_EntityMaps[entity]->GetComponentInfos();
    for (const auto& component_info : component_infos) {
        if (ranges::find(new_component_infos, component_info) == new_component_infos.end()) {
            const auto error_message = fmt::format(
                "Entity {} does not have component {}",
                entity.id,
                component_info.type_id.GetValue());
            m_World.m_Logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        std::erase_if(new_component_infos, [&](const auto& info) { return info == component_info; });
    }

    auto& new_archetype = GetOrCreateArchetype(new_component_infos);
    auto& old_archetype = *m_EntityMaps[entity];
    new_archetype.CreateEntities({entity});

    for (const auto& new_component_info : new_component_infos) {
        auto old_component_ptr = old_archetype.GetComponentData(new_component_info, entity);
        auto new_component_ptr = new_archetype.GetComponentData(new_component_info, entity);
        std::memcpy(new_component_ptr, old_component_ptr, new_component_info.size);
    }

    old_archetype.DeleteEntity(entity);
    m_EntityMaps[entity] = &new_archetype;
}

auto EntityManager::GetComponent(Entity entity, const detail::ComponentInfo& component_info) const noexcept -> std::byte* {
    if (m_EntityMaps.contains(entity)) {
        return m_EntityMaps.at(entity)->GetComponentData(component_info, entity);
    } else {
        return nullptr;
    }
}

auto EntityManager::GetDynamicComponent(Entity entity, const DynamicComponent& dynamic_component) const noexcept -> std::byte* {
    return GetComponent(entity, detail::create_component_infos<>({dynamic_component}).front());
}

void EntityManager::Destroy(Entity entity) {
    m_EntityMaps.at(entity)->DeleteEntity(entity);
    m_EntityMaps.erase(entity);
}

auto EntityManager::GetOrCreateArchetype(const detail::ComponentInfos& component_infos) noexcept -> Archetype& {
    const auto archetype_id = calculate_archetype_id(component_infos);
    if (!m_Archetypes.contains(archetype_id)) {
        m_Archetypes.emplace(archetype_id, std::make_unique<Archetype>(component_infos));
    }
    return *m_Archetypes[archetype_id];
}

auto EntityManager::GetComponentsBuffers(const detail::ComponentInfos& component_infos, Filter filter) const noexcept -> std::pmr::vector<std::pmr::vector<ComponentBuffer>> {
    filter.m_All.insert(filter.m_All.end(), component_infos.begin(), component_infos.end());

    std::pmr::vector<Archetype*> archetypes;
    for (const auto& [archetype_id, archetype] : m_Archetypes) {
        auto        p_archetype     = archetype.get();
        const auto& component_infos = p_archetype->GetComponentInfos();
        if (!filter.m_All.empty() &&
            std::find_if_not(
                filter.m_All.begin(),
                filter.m_All.end(),
                [&](const auto& component_info) {
                    return ranges::find(component_infos, component_info) != component_infos.end();
                }) !=
                filter.m_All.end()) {
            continue;
        }

        if (!filter.m_Any.empty() &&
            std::find_if(
                filter.m_Any.begin(),
                filter.m_Any.end(),
                [&](const auto& component_info) {
                    return ranges::find(component_infos, component_info) != component_infos.end();
                }) ==
                filter.m_Any.end()) {
            continue;
        }

        if (!filter.m_None.empty() &&
            std::find_if(
                filter.m_None.begin(),
                filter.m_None.end(),
                [&](const auto& component_info) {
                    return ranges::find(component_infos, component_info) != component_infos.end();
                }) !=
                filter.m_None.end()) {
            continue;
        }

        archetypes.emplace_back(p_archetype);
    }

    // [num_components, num_buffers]
    std::pmr::vector<std::pmr::vector<ComponentBuffer>> result;

    for (const auto& component_info : component_infos) {
        auto component_data = archetypes  //
                              | ranges::views::transform([&component_info](auto p_archetype) {
                                    return p_archetype->GetComponentBuffers(component_info);
                                })                   //
                              | ranges::views::join  //
                              | ranges::to<std::pmr::vector<ComponentBuffer>>();
        result.emplace_back(std::move(component_data));
    }

    assert(ranges::all_of(result, [&](const auto& component_data) { return component_data.size() == result.front().size(); }));
    for (auto& component_buffers : result) {
        assert(ranges::all_of(component_buffers, [&](const auto& component_buffer) { return component_buffer.second == component_buffers.front().second; }));
    }

    return result;
}

}  // namespace hitagi::ecs