#pragma once
#include <hitagi/ecs/entity_manager.hpp>

namespace spdlog {
class logger;
}

namespace hitagi::ecs {
class Schedule;

template <typename T>
concept SystemLike = requires(Schedule& s) {
    { T::OnUpdate(s) } -> std::same_as<void>;
};

class World {
public:
    World(std::string_view name);

    void Update();

    template <SystemLike System>
    void RegisterSystem(std::string_view name);

    inline auto& GetEntityManager() noexcept { return m_EntityManager; }
    inline auto& GetEntityManager() const noexcept { return m_EntityManager; }

private:
    friend class EntityManager;
    void WarnMessage(std::string_view message);

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    EntityManager m_EntityManager;

    std::pmr::unordered_map<std::pmr::string, std::function<void(Schedule&)>> m_Systems;
};

template <SystemLike System>
void World::RegisterSystem(std::string_view name) {
    std::pmr::string _name{name};
    if (m_Systems.contains(std::pmr::string(_name))) {
        WarnMessage(fmt::format("The system ({}) existed!", name));
    } else
        m_Systems.emplace(_name, &System::OnUpdate);
}

}  // namespace hitagi::ecs