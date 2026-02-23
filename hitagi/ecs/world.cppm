module;
#include <taskflow/taskflow.hpp>
#include <spdlog/logger.h>

export module ecs:world;
import std;
import :types;
import :entity_manager;
import :system_manager;

export namespace hitagi::ecs {

class World {
public:
    World(std::string_view name);

    void Update();

    inline auto& GetEntityManager() noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() noexcept { return m_SystemManager; }
    inline auto& GetEntityManager() const noexcept { return m_EntityManager; }
    inline auto& GetSystemManager() const noexcept { return m_SystemManager; }
    inline auto  GetLogger() noexcept { return m_Logger; }

private:
    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    EntityManager m_EntityManager;
    SystemManager m_SystemManager;
    tf::Executor  m_Executor;
};

}  // namespace hitagi::ecs
