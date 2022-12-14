#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

using namespace hitagi;

class ImGuiDemo : public hitagi::RuntimeModule {
public:
    ImGuiDemo() : hitagi::RuntimeModule("ImGuiDemo") {}
    void Tick() final;
};