#pragma once
#include <hitagi/render/renderer.hpp>
#include <hitagi/render/utils.hpp>
#include <hitagi/application.hpp>
#include <hitagi/render_graph/render_graph.hpp>

namespace hitagi::render {

class ForwardRenderer : public IRenderer {
public:
    ForwardRenderer(gfx::Device& device, const Application& app, gui::GuiManager* gui_manager = nullptr, std::string_view name = "");

    void Tick() override;

    void RenderScene(std::shared_ptr<asset::Scene> scene, std::shared_ptr<asset::CameraNode> camera, rg::TextureHandle target) override;

    void RenderGui(rg::TextureHandle target, bool clear_target) override;

    void ToSwapChain(rg::TextureHandle from) override;

    inline auto GetFrameTime() const noexcept -> std::chrono::duration<double> override { return m_Clock.DeltaTime(); }

    inline auto GetRenderGraph() noexcept -> rg::RenderGraph& override { return m_RenderGraph; }

    auto GetSwapChain() const noexcept -> gfx::SwapChain& final { return *m_SwapChain; }

private:
    const Application& m_App;
    gfx::Device&       m_GfxDevice;

    core::Clock m_Clock;

    std::shared_ptr<gfx::SwapChain> m_SwapChain;
    rg::RenderGraph                 m_RenderGraph;

    std::unique_ptr<GuiRenderUtils> m_GuiRenderUtils;
    rg::TextureHandle               m_GuiTarget;
    bool                            m_ClearGuiTarget = false;
};

}  // namespace hitagi::render