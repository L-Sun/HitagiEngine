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

    // Render scene to texture which is back buffer if `texture_size == nullopt` else a new texture
    virtual auto RenderScene(const asset::Scene& scene, const asset::CameraNode& camera, std::optional<gfx::ViewPort> viewport = std::nullopt, std::optional<math::vec2u> texture_size = std::nullopt) -> gfx::ResourceHandle = 0;

    // Render gui to target which is back buffer if the handle is invalid
    virtual auto RenderGui(std::optional<gfx::ResourceHandle> target = std::nullopt) -> gfx::ResourceHandle = 0;

    // Get frame time
    virtual auto GetFrameTime() const noexcept -> std::chrono::duration<double> = 0;
};

}  // namespace hitagi::render