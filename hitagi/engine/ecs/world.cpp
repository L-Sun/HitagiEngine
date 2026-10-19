module;
#include "interop/tracy_macros.hpp"

export module ecs:world;
import interop.spdlog;
import interop.tracy;

import std;
import core;
import utils;

import :entity_manager;
import :system_manager;
import :schedule;

export namespace hitagi::ecs {

class World {
public:
    explicit World(std::string_view name);
    ~World();

    template <typename... Systems>
    void RegisterSystem() { (RegisterOne<Systems>(), ...); }

    // System-set changes are reflected in the next Update, never by destroying
    // a running schedule. Concurrent registry mutation is not supported.
    void Update();
    void Update(core::JobSystem& job_system);

    auto  GetName() const noexcept -> std::string_view { return m_Name; }
    auto& GetEntityManager() noexcept { return m_EntityManager; }
    auto& GetSystemManager() noexcept { return m_SystemManager; }
    auto& GetEntityManager() const noexcept { return m_EntityManager; }
    auto& GetSystemManager() const noexcept { return m_SystemManager; }
    auto  GetLogger() noexcept { return m_Logger; }

private:
    template <typename System>
    void RegisterOne();
    auto PrepareSchedule() -> Schedule&;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;
    EntityManager                   m_EntityManager;
    SystemManager                   m_SystemManager;
    std::unique_ptr<Schedule>       m_Schedule;
    std::uint64_t                   m_ScheduleRevision = 0;
    bool                            m_Updating         = false;
};

template <typename System>
void World::RegisterOne() {
    detail::SystemCallbacks callbacks;
    if constexpr (requires(World& world) { { System::OnCreate(world) } -> std::same_as<void>; })
        callbacks.on_create = [this] { System::OnCreate(*this); };
    if constexpr (requires(World& world) { { System::OnEnable(world) } -> std::same_as<void>; })
        callbacks.on_enable = [this] { System::OnEnable(*this); };
    if constexpr (requires(Schedule& schedule) { { System::OnUpdate(schedule) } -> std::same_as<void>; })
        callbacks.on_update = &System::OnUpdate;
    if constexpr (requires(World& world) { { System::OnDisable(world) } -> std::same_as<void>; })
        callbacks.on_disable = [this] { System::OnDisable(*this); };
    if constexpr (requires(World& world) { { System::OnDestroy(world) } -> std::same_as<void>; })
        callbacks.on_destroy = [this] { System::OnDestroy(*this); };
    m_SystemManager.Register(utils::TypeID::Create<System>(), std::move(callbacks));
}

}  // namespace hitagi::ecs

namespace hitagi::ecs {

World::World(std::string_view name)
    : m_Name(name), m_Logger(utils::try_create_logger(name)), m_EntityManager(m_Logger) {}

World::~World() {
    // Lifecycle callbacks may still access entities and other world services.
    m_SystemManager.Shutdown();
    m_Schedule.reset();
}

auto World::PrepareSchedule() -> Schedule& {
    const auto revision = m_SystemManager.m_Revision;
    if (!m_Schedule || m_ScheduleRevision != revision) {
        auto schedule = std::unique_ptr<Schedule>(new Schedule(m_EntityManager.m_Storage, m_Logger, m_Name));
        m_SystemManager.Update(*schedule);
        m_Schedule = std::move(schedule);
        // A callback may have changed the registry while building this snapshot.
        m_ScheduleRevision = revision;
    }
    return *m_Schedule;
}

void World::Update() {
    if (m_Updating) throw std::logic_error("World::Update cannot be called recursively");
    m_Updating = true;
    try {
        ZoneScopedN("ECS::World::Update");
        PrepareSchedule().RunSerial();
    } catch (...) {
        m_Updating = false;
        throw;
    }
    m_Updating = false;
}

void World::Update(core::JobSystem& job_system) {
    if (m_Updating) throw std::logic_error("World::Update cannot be called recursively");
    m_Updating = true;
    try {
        ZoneScopedN("ECS::World::Update");
        PrepareSchedule().Run(job_system);
    } catch (...) {
        m_Updating = false;
        throw;
    }
    m_Updating = false;
}

}  // namespace hitagi::ecs
