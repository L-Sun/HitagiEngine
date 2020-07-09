#include "Application.hpp"
#include "MyTest.hpp"
#include "HitagiPhysicsManager.hpp"

namespace Hitagi {
GfxConfiguration                          config("MyTest", 8, 8, 8, 8, 24, 8, 0, 1280, 720);
std::unique_ptr<Physics::IPhysicsManager> g_PhysicsManager = std::make_unique<Physics::HitagiPhysicsManager>();
std::unique_ptr<GameLogic>                g_GameLogic      = std::make_unique<MyTest>();
}  // namespace Hitagi