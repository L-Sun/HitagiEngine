module;
#include <taskflow/taskflow.hpp>
#include <spdlog/logger.h>

export module ecs:world;
import std;
import :types;
import :entity_manager;
import :system_manager;

export namespace hitagi::ecs {

class Schedule;

class World {
public:
    World(std::string_view name);
    ~World();

    void Update();

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto& GetEntityManager() noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() noexcept { return m_SystemManager; }
    inline auto& GetEntityManager() const noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() const noexcept { return m_SystemManager; }
    inline auto  GetLogger() noexcept { return m_Logger; }

private:
    friend SystemManager;

    void InvalidateSchedule() noexcept;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    EntityManager             m_EntityManager;
    std::unique_ptr<Schedule> m_Schedule;
    bool                     m_ScheduleDirty = true;
    tf::Executor             m_Executor;
    SystemManager            m_SystemManager;
};

}  // namespace hitagi::ecs
