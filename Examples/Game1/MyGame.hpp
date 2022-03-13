#pragma once
#include "GameLogic.hpp"
#include "Editor.hpp"

class MyGame : public Hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
    Hitagi::Core::Clock m_Clock;
    Hitagi::Editor      m_Editor;
};