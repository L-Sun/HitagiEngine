#pragma once
#include <hitagi/core/runtime_module.hpp>

namespace hitagi {
class GameLogic : public IRuntimeModule {
public:
    int  Initialize() override = 0;
    void Finalize() override   = 0;
    void Tick() override       = 0;
};
extern std::unique_ptr<GameLogic> g_GameLogic;
}  // namespace hitagi
