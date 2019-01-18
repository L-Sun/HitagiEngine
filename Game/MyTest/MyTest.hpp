#pragma once
#include "GameLogic.hpp"

namespace My {
class MyTest : public GameLogic {
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void OnLeftKey() final;
    void OnDKey() final;
};
}  // namespace My