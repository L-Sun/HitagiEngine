#pragma once
#include <hitagi/core/runtime_module.hpp>

class ImGuiDemo : public hitagi::RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
};