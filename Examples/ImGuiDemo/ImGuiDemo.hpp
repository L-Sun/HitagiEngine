#pragma once
#include "GameLogic.hpp"

class ImGuiDemo : public hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
};