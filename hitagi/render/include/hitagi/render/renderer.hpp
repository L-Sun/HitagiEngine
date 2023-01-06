#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/asset/scene.hpp>
#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gui/gui_manager.hpp>

namespace hitagi::render {
class IRenderer : public RuntimeModule {
public:
    using RuntimeModule::RuntimeModule;

    // Render a scene in the viewport given camera
    virtual auto RenderScene(const asset::Scene& scene, const gfx::ViewPort& viewport, const asset::CameraNode& camera) -> gfx::ResourceHandle = 0;

    virtual auto GetFrameTime() const noexcept -> std::chrono::duration<double> = 0;
};

}  // namespace hitagi::render