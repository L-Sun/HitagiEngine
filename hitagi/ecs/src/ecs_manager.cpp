#include <hitagi/ecs/ecs_manager.hpp>
#include <memory_resource>

namespace hitagi::ecs {

int  EcsManager::Initialize() { return 0; }
void EcsManager::Tick() {}
void EcsManager::Finalize() {}

std::shared_ptr<World> EcsManager::CreateEcsWorld(std::string_view name) {
    auto world = std::allocate_shared<World>(
        std::pmr::polymorphic_allocator<World>(std::pmr::get_default_resource()));

    m_Worlds.emplace(name, world);
    return world;
}

}  // namespace hitagi::ecs