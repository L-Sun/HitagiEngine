module;
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

module ecs;
import std;
namespace hitagi::ecs {
World::World(std::string_view name, core::JobSystem* job_system)
    : m_Name(name),
      m_Logger(utils::try_create_logger(name)),
      m_EntityManager(*this),
      m_JobSystem(job_system),
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

    if (m_JobSystem == nullptr) {
        m_JobSystem = core::JobSystem::Get();
    }
    if (m_JobSystem == nullptr) {
        throw std::runtime_error("ecs::World requires core::JobSystem to update");
    }

    m_Schedule->Run(*m_JobSystem);
}

void World::InvalidateSchedule() noexcept {
    m_ScheduleDirty = true;
    m_Schedule.reset();
}

}  // namespace hitagi::ecs
