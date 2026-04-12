module;
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

module ecs;
import std;
import :schedule;

namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name),
      m_Logger(utils::try_create_logger(name)),
      m_EntityManager(*this),
      m_SystemManager(*this) {
}

void World::Update() {
    ZoneScopedN("ECS::World::Update");
    Schedule schedule(*this);
    m_SystemManager.Update(schedule);
    schedule.Run(m_Executor);
}

}  // namespace hitagi::ecs
