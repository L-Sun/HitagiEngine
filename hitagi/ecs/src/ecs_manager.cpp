#include <hitagi/ecs/ecs_manager.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::ecs {

bool EcsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("EcsManager");
    m_Logger->info("Initialize...");
    return true;
}
void EcsManager::Tick() {}
void EcsManager::Finalize() {
    m_Logger->info("Finalize...");
    m_Logger = nullptr;
}

std::shared_ptr<World> EcsManager::CreateEcsWorld(std::string_view name) {
    auto world = std::make_shared<World>();

    m_Worlds.emplace(name, world);
    return world;
}

}  // namespace hitagi::ecs