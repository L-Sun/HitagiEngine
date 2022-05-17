#include <hitagi/ecs/ecs_manager.hpp>
#include <hitagi/core/memory_manager.hpp>

namespace hitagi::ecs {

int  EcsManager::Initialize() { return 0; }
void EcsManager::Tick() {}
void EcsManager::Finalize() {}

std::shared_ptr<World> EcsManager::CreateEcsWorld(std::string_view name) {
    auto world = std::make_shared<World>();

    m_Worlds.emplace(name, world);
    return world;
}

}  // namespace hitagi::ecs