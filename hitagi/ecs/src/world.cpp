#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/logger.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name),
      m_Logger(utils::try_create_logger(name)),
      m_EntityManager(*this) {
}

void World::Update() {
    Schedule schedule(*this);

    for (auto&& [name, update_fn] : m_Systems) {
        update_fn(schedule);
    }

    schedule.Run();
}

void World::WarnMessage(std::string_view message) {
    m_Logger->warn(message);
}

}  // namespace hitagi::ecs