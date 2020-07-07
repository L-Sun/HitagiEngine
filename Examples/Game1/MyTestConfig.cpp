#include "Application.hpp"
#include "MyTest.hpp"
#include "HitagiPhysicsManager.hpp"

namespace Hitagi {
// clang-format off
GfxConfiguration config("MyTest", 8, 8, 8, 8, 24, 8, 0, 800, 600);
std::unique_ptr<Physics::IPhysicsManager> g_PhysicsManager=std::make_unique<Physics::HitagiPhysicsManager>(); 
std::unique_ptr<GameLogic>       g_GameLogic=std::make_unique<MyTest>();
// clang-format on
}  // namespace Hitagi