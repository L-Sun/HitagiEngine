#include "GfxConfiguration.h"
#include "MyTest.hpp"
#include "My/MyPhysicsManager.hpp"

namespace My {
// clang-format off
GfxConfiguration config("MyTest", 8, 8, 8, 8, 24, 8, 0, 1024, 720);
IPhysicsManager* g_pPhysicsManager = static_cast<IPhysicsManager*>(new MyPhysicsManager); 
GameLogic*       g_pGameLogic      = static_cast<GameLogic*>(new MyTest);
// clang-format on
}  // namespace My