#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name) {
    if (auto logger = spdlog::get(std::string{name}); logger)
        m_Logger = logger;
    else
        m_Logger = spdlog::stdout_color_mt(name.data());

    m_Logger->debug("Create {} world!", name);
    m_Timer.Start();
}

void World::Update() {
    Schedule schedule(*this);

    for (auto&& [name, update_fn] : m_Systems) {
        update_fn(schedule, m_Timer.DeltaTime());
    }
    m_Timer.Tick();

    schedule.Run();
}

void World::DestoryEntity(const Entity& entity) {
    if (!m_EnitiesMap.contains(entity))
        return;

    m_EnitiesMap.at(entity)->DeleteInstance(entity);
}

void World::LogMessage(std::string_view message) {
    m_Logger->debug(message);
}

}  // namespace hitagi::ecs