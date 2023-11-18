#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/logger.hpp>

#include <range/v3/view/map.hpp>
#include <spdlog/spdlog.h>

namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name),
      m_Logger(utils::try_create_logger(name)),
      m_EntityManager(*this) {
}

void World::Update() {
    Schedule schedule(*this);

    for (auto&& update_fn : m_Systems | ranges::views::values) {
        update_fn(schedule);
    }

    schedule.Run();
}

}  // namespace hitagi::ecs