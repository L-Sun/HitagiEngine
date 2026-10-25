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
    using ScheduleConfiguration = std::function<void(Schedule&)>;
    explicit World(std::string_view name, ScheduleConfiguration configure_schedule = {});
    ~World();

    template <typename First, typename Second, typename... Rest>
    void RegisterSystem() {
        RegisterOne<First>();
        RegisterOne<Second>();
        (RegisterOne<Rest>(), ...);
    }

    template <typename System, typename... Args>
    auto RegisterSystem(Args&&... args) -> System& { return RegisterOne<System>(std::forward<Args>(args)...); }

    // Runs the cached schedule on the supplied job system and waits for it.
    // System-set changes are reflected in the next Update, never by destroying
    // a running schedule. Concurrent registry mutation is not supported.
    void Update(core::JobSystem& job_system, double delta_seconds = 0.0);

    auto  GetName() const noexcept -> std::string_view { return m_Name; }
    auto& GetEntityManager() noexcept { return m_EntityManager; }
    auto& GetSystemManager() noexcept { return m_SystemManager; }
    auto& GetEntityManager() const noexcept { return m_EntityManager; }
    auto& GetSystemManager() const noexcept { return m_SystemManager; }
    auto  GetLogger() noexcept { return m_Logger; }

private:
    template <typename System, typename... Args>
    auto RegisterOne(Args&&... args) -> System&;
    auto PrepareSchedule() -> Schedule&;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;
    EntityManager                   m_EntityManager;
    SystemManager                   m_SystemManager;
    ScheduleConfiguration           m_ConfigureSchedule;
    std::unique_ptr<Schedule>       m_Schedule;
    std::uint64_t                   m_ScheduleRevision = 0;
    bool                            m_Updating         = false;
};

template <typename System, typename... Args>
auto World::RegisterOne(Args&&... args) -> System& {
    if (m_SystemManager.m_ShuttingDown) throw std::logic_error("Cannot register a system during shutdown");
    const auto id = utils::TypeID::Create<System>();
    if (const auto found = m_SystemManager.m_Systems.find(id); found != m_SystemManager.m_Systems.end()) {
        auto instance = std::static_pointer_cast<System>(found->second.state);
        m_SystemManager.EnableOne(id);
        const auto current = m_SystemManager.m_Systems.find(id);
        if (current == m_SystemManager.m_Systems.end() || current->second.state != instance)
            throw std::logic_error("System removed or replaced itself during registration");
        return *instance;
    }
    auto                    instance = std::make_shared<System>(std::forward<Args>(args)...);
    detail::SystemCallbacks callbacks;
    if constexpr (requires { instance->OnCreate(*this); })
        callbacks.on_create = [this, instance] { instance->OnCreate(*this); };
    if constexpr (requires { instance->OnEnable(*this); })
        callbacks.on_enable = [this, instance] { instance->OnEnable(*this); };
    if constexpr (requires(Schedule& schedule) { instance->ConfigureSchedule(schedule); })
        callbacks.configure_schedule = [instance](Schedule& schedule) {
            schedule.m_SystemStates.emplace_back(instance);
            instance->ConfigureSchedule(schedule);
        };
    if constexpr (requires { instance->OnDisable(*this); })
        callbacks.on_disable = [this, instance] { instance->OnDisable(*this); };
    if constexpr (requires { instance->OnDestroy(*this); })
        callbacks.on_destroy = [this, instance] { instance->OnDestroy(*this); };

    m_SystemManager.Register(id, std::move(callbacks), instance);

    const auto current = m_SystemManager.m_Systems.find(id);
    if (current == m_SystemManager.m_Systems.end() || current->second.state != instance)
        throw std::logic_error("System removed or replaced itself during registration");
    return *instance;
}

}  // namespace hitagi::ecs

namespace hitagi::ecs {

World::World(std::string_view name, ScheduleConfiguration configure_schedule)
    : m_Name(name), m_Logger(utils::try_create_logger(name)), m_EntityManager(m_Logger), m_ConfigureSchedule(std::move(configure_schedule)) {}

World::~World() {
    // Lifecycle callbacks may still access entities and other world services.
    m_Schedule.reset();
    m_SystemManager.Shutdown();
}

auto World::PrepareSchedule() -> Schedule& {
    const auto revision = m_SystemManager.m_Revision;
    if (!m_Schedule || m_ScheduleRevision != revision) {
        auto schedule = std::unique_ptr<Schedule>(new Schedule(m_EntityManager.m_Storage, m_Logger, m_Name));
        if (m_ConfigureSchedule) m_ConfigureSchedule(*schedule);
        m_SystemManager.Update(*schedule);
        m_Schedule = std::move(schedule);
        // A callback may have changed the registry while building this snapshot.
        m_ScheduleRevision = revision;
    }
    return *m_Schedule;
}

void World::Update(core::JobSystem& job_system, double delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0) throw std::invalid_argument("Invalid frame duration");
    if (m_Updating) throw std::logic_error("World::Update cannot be called recursively");
    m_Updating = true;
    try {
        ZoneScopedN("ECS::World::Update");
        auto& schedule          = PrepareSchedule();
        schedule.m_DeltaSeconds = delta_seconds;
        schedule.Run(job_system);
    } catch (...) {
        m_Updating = false;
        throw;
    }
    m_Updating = false;
}

}  // namespace hitagi::ecs
