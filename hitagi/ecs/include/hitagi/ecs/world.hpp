#pragma once
#include <hitagi/ecs/entity_manager.hpp>

namespace spdlog {
class logger;
}

namespace hitagi::ecs {
class Schedule;

template <typename T>
concept SystemLike = requires(World& world, Schedule& schedule) {
    { T::OnRegister(world) } -> std::same_as<void>;
    { T::OnUpdate(schedule) } -> std::same_as<void>;
    { T::OnUnregister(world) } -> std::same_as<void>;
};

class World {
public:
    World(std::string_view name);

    void Update();

    template <SystemLike System>
    void RegisterSystem();

    template <SystemLike System>
    void UnregisterSystem();

    inline auto& GetEntityManager() noexcept { return m_EntityManager; }
    inline auto& GetEntityManager() const noexcept { return m_EntityManager; }

private:
    friend class EntityManager;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    EntityManager m_EntityManager;

    std::pmr::unordered_map<std::size_t, std::function<void(Schedule&)>> m_Systems;
};

template <SystemLike System>
void World::RegisterSystem() {
    bool success = m_Systems.emplace(typeid(System::OnUpdate).hash_code(), &System::OnUpdate).second;
    if (success) System::OnRegister(*this);
}

template <SystemLike System>
void World::UnregisterSystem() {
    std::size_t num_erased = m_Systems.erase(typeid(System::OnUpdate).hash_code());
    if (num_erased != 0) System::OnUnregister(*this);
}

}  // namespace hitagi::ecs