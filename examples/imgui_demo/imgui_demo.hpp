#pragma once
#include <hitagi/gameplay/gamelogic.hpp>

class ImGuiDemo : public hitagi::GameLogic {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
};