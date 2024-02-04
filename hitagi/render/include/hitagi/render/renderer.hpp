#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/asset/scene.hpp>
#include <hitagi/render_graph/render_graph.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gui/gui_manager.hpp>

namespace hitagi::render {
class IRenderer : public RuntimeModule {
public:
    using RuntimeModule::RuntimeModule;

    virtual ~IRenderer() = default;

    // Render scene to texture
    virtual void RenderScene(std::shared_ptr<asset::Scene> scene, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) = 0;

    virtual void RenderGui(rg::TextureHandle target, bool clear_target) = 0;

    virtual void ToSwapChain(rg::TextureHandle from) = 0;

    // Get frame time
    virtual auto GetFrameTime() const noexcept -> std::chrono::duration<double> = 0;

    // Get the render graph
    virtual auto GetRenderGraph() noexcept -> rg::RenderGraph& = 0;

    virtual auto GetSwapChain() const noexcept -> gfx::SwapChain& = 0;
};

}  // namespace hitagi::render