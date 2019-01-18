#include "GfxConfiguration.h"
#include "BilliardGameLogic.hpp"
#include "Bullet/BulletPhysicsManager.hpp"

namespace My {
// clang-format off
GfxConfiguration config("Billiard", 8, 8, 8, 8, 24, 8, 0, 960, 540);
IPhysicsManager* g_pPhysicsManager = static_cast<IPhysicsManager*>(new BulletPhysicsManager); 
GameLogic*       g_pGameLogic      = static_cast<GameLogic*>(new BilliardGameLogic);
// clang-format on
}  // namespace My