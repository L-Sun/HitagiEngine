#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/schedule.hpp>

#include <unordered_map>

namespace hitagi::ecs {
class EcsManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    std::shared_ptr<World> CreateEcsWorld(std::string_view name);

private:
    std::pmr::unordered_map<std::string, std::shared_ptr<World>> m_Worlds;
};
}  // namespace hitagi::ecs