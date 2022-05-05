#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>

namespace hitagi::ecs {

void World::Update() {
    Schedule schedule(*this);

    for (auto&& system_fn : m_Systems) {
        system_fn(schedule);
    }

    schedule.Run();
}

void World::DestoryEntity(const Entity& entity) {
    if (m_EnitiesMap.count(entity) == 0)
        return;

    m_EnitiesMap.at(entity)->DeleteInstance(entity);
}

}  // namespace hitagi::ecs