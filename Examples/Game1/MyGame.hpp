#pragma once
#include "GameLogic.hpp"
#include "Editor.hpp"

class MyGame : public Hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void DrawBone();
    void DumpAnimation();

private:
    bool           m_ShowEditor = true;
    Hitagi::Editor m_Editor;
};