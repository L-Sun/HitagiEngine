#include "GfxConfiguration.h"
#include "HitagiTest.hpp"
#include "HitagiPhysicsManager.hpp"

namespace Hitagi {
// clang-format off
GfxConfiguration config("HitagiTest", 8, 8, 8, 8, 24, 8, 0, 1024, 720);
std::unique_ptr<IPhysicsManager> g_hysicsManager(new HitagiPhysicsManager); 
std::unique_ptr<GameLogic>       g_GameLogic(new HitagiTest);
// clang-format on
}  // namespace Hitagi