#pragma once
#include "GameLogic.hpp"

namespace Hitagi {
class MyTest : public GameLogic {
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
    enum {
        DEBUG_TOGGLE,
        ZOOM,
        ROTATE_ON,
        ROTATE_Z,
        ROTATE_H,
        MOVE_UP,
        MOVE_DOWN,
        MOVE_LEFT,
        MOVE_RIGHT,
        MOVE_FRONT,
        MOVE_BACK,
        RESET_SCENE,
        MSAA
    };
    float sensitivity = 0.5f;
};
}  // namespace Hitagi