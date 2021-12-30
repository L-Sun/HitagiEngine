#pragma once
#include "GameLogic.hpp"

class ImGuiDemo : public Hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
};