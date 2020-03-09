#include "IApplication.hpp"
#include "MyTest.hpp"
#include "HitagiPhysicsManager.hpp"

namespace Hitagi {
// clang-format off
GfxConfiguration config("MyTest", 8, 8, 8, 8, 24, 8, 0, 1024, 720);
std::unique_ptr<Physics::IPhysicsManager> g_PhysicsManager(new Physics::HitagiPhysicsManager); 
std::unique_ptr<GameLogic>       g_GameLogic(new MyTest);
// clang-format on
}  // namespace Hitagi