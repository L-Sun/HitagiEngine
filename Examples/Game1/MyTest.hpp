#pragma once
#include "GameLogic.hpp"

class MyTest : public Hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
    enum {
        DEBUG_TOGGLE,
        MOVE_FRONT,
        MOVE_BACK,
        MOVE_LEFT,
        MOVE_RIGHT,
        MOVE_UP,
        MOVE_DOWN,
        ROTATE_H,
        ROTATE_V,
        RESET_SCENE,
        MSAA
    };
    Hitagi::Core::Clock m_Clock;
};