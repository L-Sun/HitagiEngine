#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name),
      m_EntityManager(*this) {
    if (auto logger = spdlog::get(std::string{name}); logger)
        m_Logger = logger;
    else
        m_Logger = spdlog::stdout_color_mt(name.data());

    m_Logger->debug("Create {} world!", name);
}

void World::Update() {
    Schedule schedule(*this);

    for (auto&& [name, update_fn] : m_Systems) {
        update_fn(schedule);
    }

    schedule.Run();
}

void World::LogMessage(std::string_view message) {
    m_Logger->debug(message);
}

}  // namespace hitagi::ecs