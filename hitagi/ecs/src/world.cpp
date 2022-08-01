#pragma once
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>

namespace hitagi::ecs {
World::World(std::string_view name) : m_Name(name) {
    m_Timer.Start();
}

void World::Update() {
    Schedule schedule(*this);

    for (auto&& system_fn : m_Systems) {
        system_fn(schedule, m_Timer.DeltaTime());
    }
    m_Timer.Tick();

    schedule.Run();
}

void World::DestoryEntity(const Entity& entity) {
    if (m_EnitiesMap.count(entity) == 0)
        return;

    m_EnitiesMap.at(entity)->DeleteInstance(entity);
}

}  // namespace hitagi::ecs