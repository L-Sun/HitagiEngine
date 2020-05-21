#pragma once
#include "IRuntimeModule.hpp"
#include "IPhysicsManager.hpp"
#include "SceneManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"
#include "GraphicsManager.hpp"
#include "Timer.hpp"

#include <spdlog/spdlog.h>

namespace Hitagi {

class GameLogic : public IRuntimeModule {
public:
    int  Initialize() override;
    void Finalize() override;
    void Tick() override;
};
extern std::unique_ptr<GameLogic> g_GameLogic;
}  // namespace Hitagi
