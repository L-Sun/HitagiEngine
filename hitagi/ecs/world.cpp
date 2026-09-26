module;
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

module ecs;
import std;
namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name),
      m_Logger(utils::try_create_logger(name)),
      m_EntityManager(*this),
      m_SystemManager(*this) {
}

World::~World() = default;

auto World::PrepareSchedule() -> Schedule& {
    if (m_ScheduleDirty || !m_Schedule) {
        m_Schedule = std::make_unique<Schedule>(*this);
        m_SystemManager.Update(*m_Schedule);
        m_ScheduleDirty = false;
    }
    return *m_Schedule;
}

void World::Update() {
    ZoneScopedN("ECS::World::Update");
    PrepareSchedule().RunSerial();
}

void World::Update(core::JobSystem& job_system) {
    ZoneScopedN("ECS::World::Update");
    PrepareSchedule().Run(job_system);
}

void World::InvalidateSchedule() noexcept {
    m_ScheduleDirty = true;
    m_Schedule.reset();
}

}  // namespace hitagi::ecs
