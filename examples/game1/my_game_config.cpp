#include "my_game.hpp"
#include <hitagi/application.hpp>
#include <hitagi/physics/hitagi_physics.hpp>

namespace hitagi {
GfxConfiguration                          g_Config("MyGame", 8, 8, 8, 8, 24, 8, 0, 1280, 720);
std::unique_ptr<physics::IPhysicsManager> g_PhysicsManager = std::make_unique<physics::hitagiPhysicsManager>();
std::unique_ptr<GameLogic>                g_GameLogic      = std::make_unique<MyGame>();
}  // namespace hitagi