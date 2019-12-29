#pragma once
#include "GameLogic.hpp"
#include <string>
#include <vector>

namespace My {
class MyTest : public GameLogic {
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void OnLeftKey() final;
    void OnRightKey() final;
    void OnUpKey() final;
    void OnDownKey() final;
    void OnCKey() final;

private:
    std::vector<std::string> selectedNode = {"Sphere", "Cone", "Suzanne",
                                             "Cube"};

    size_t i = 0;
};
}  // namespace My