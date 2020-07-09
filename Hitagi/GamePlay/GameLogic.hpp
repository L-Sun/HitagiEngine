#pragma once
#include "IRuntimeModule.hpp"
#include "AssetManager.hpp"
#include "SceneManager.hpp"
#include "GraphicsManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"
#include "Timer.hpp"

namespace Hitagi {
class GameLogic : public IRuntimeModule {
public:
    int  Initialize() = 0;
    void Finalize()   = 0;
    void Tick()       = 0;
};
extern std::unique_ptr<GameLogic> g_GameLogic;
}  // namespace Hitagi
