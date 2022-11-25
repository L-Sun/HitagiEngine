#include <hitagi/ecs/entity_manager.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/logger.h>
#include <fmt/color.h>

namespace hitagi::ecs {

void EntityManager::Destory(Entity entity) noexcept {
    if (m_EntityMaps.contains(entity)) {
        m_EntityMaps[entity]->DeleteEntity(entity);
        m_EntityMaps.erase(entity);
    }
}

auto EntityManager::GetArchetype(const Filter& filter) const -> std::pmr::vector<detials::IArchetype*> {
    std::pmr::vector<detials::IArchetype*> result;

    for (const auto& [archetype_id, archetype] : m_Archetypes) {
        auto p_archetype = archetype.get();
        if (!filter.all.empty() &&
            std::find_if_not(
                filter.all.begin(),
                filter.all.end(),
                [&](const utils::TypeID& type) { return p_archetype->HasComponent(type); }) !=
                filter.all.end()) {
            continue;
        }

        if (!filter.any.empty() &&
            std::find_if(
                filter.any.begin(),
                filter.any.end(),
                [&](const utils::TypeID& type) { return p_archetype->HasComponent(type); }) ==
                filter.any.end()) {
            continue;
        }

        if (!filter.none.empty() &&
            std::find_if(
                filter.none.begin(),
                filter.none.end(),
                [&](const utils::TypeID& type) { return p_archetype->HasComponent(type); }) !=
                filter.none.end()) {
            continue;
        }

        result.emplace_back(p_archetype);
    }

    return result;
}

}  // namespace hitagi::ecs