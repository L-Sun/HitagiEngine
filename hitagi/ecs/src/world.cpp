#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>

namespace hitagi::ecs {
World::World(allocator_type alloc)
    : m_Allocator(alloc),
      m_EnitiesMap(alloc),
      m_Archetypes(alloc),
      m_Systems(alloc) {
    m_Timer.Start();
}

void World::Update() {
    Schedule schedule(*this, m_Allocator);

    for (auto&& system_fn : m_Systems) {
        system_fn(schedule, m_Timer.DeltaTime());
    }

    schedule.Run();
}

void World::DestoryEntity(const Entity& entity) {
    if (m_EnitiesMap.count(entity) == 0)
        return;

    m_EnitiesMap.at(entity)->DeleteInstance(entity);
}

}  // namespace hitagi::ecs