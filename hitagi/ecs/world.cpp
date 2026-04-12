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

World::~World() = default;

void World::Update() {
    ZoneScopedN("ECS::World::Update");
    if (m_ScheduleDirty || !m_Schedule) {
        m_Schedule = std::make_unique<Schedule>(*this);
        m_SystemManager.Update(*m_Schedule);
        m_ScheduleDirty = false;
    }

    m_Schedule->Run(m_Executor);
}

void World::InvalidateSchedule() noexcept {
    m_ScheduleDirty = true;
    m_Schedule.reset();
}

}  // namespace hitagi::ecs
