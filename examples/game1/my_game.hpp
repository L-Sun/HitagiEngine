#pragma once
#include <hitagi/core/timer.hpp>
#include <hitagi/gameplay.hpp>

class MyGame : public hitagi::GamePlay {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
    hitagi::core::Clock m_Clock;
};