#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/logger.hpp>

#include <range/v3/view/map.hpp>
#include <spdlog/spdlog.h>

namespace hitagi::ecs {
World::World(std::string_view name)
    : m_Name(name),
      m_Logger(utils::try_create_logger(name)),
      m_EntityManager(*this),
      m_SystemManager(*this) {
}

void World::Update() {
    Schedule schedule(*this);
    m_SystemManager.Update(schedule);
    schedule.Run(m_Executor);
}

}  // namespace hitagi::ecs