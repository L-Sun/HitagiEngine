#pragma once
#include <hitagi/ecs/entity_manager.hpp>
#include <hitagi/ecs/system_manager.hpp>

#include <fmt/format.h>
#include <taskflow/taskflow.hpp>

namespace spdlog {
class logger;
}

namespace hitagi::ecs {
class Schedule;

class World {
public:
    World(std::string_view name);

    void Update();

    inline auto& GetEntityManager() noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() noexcept { return m_SystemManager; }
    inline auto  GetLogger() noexcept { return m_Logger; }

private:
    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    EntityManager m_EntityManager;
    SystemManager m_SystemManager;
    tf::Executor  m_Executor;
};

}  // namespace hitagi::ecs