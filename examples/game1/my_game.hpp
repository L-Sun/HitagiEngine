#pragma once
#include <hitagi/core/timer.hpp>
#include <hitagi/gameplay/gamelogic.hpp>
#include <hitagi/editor/editor.hpp>

class MyGame : public hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
    void DrawBone();

    hitagi::core::Clock m_Clock;
    hitagi::Editor      m_Editor;
};