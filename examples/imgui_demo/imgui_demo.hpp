#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

using namespace hitagi;

class ImGuiDemo : public hitagi::RuntimeModule {
public:
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "ImGuiDemo"; }
};