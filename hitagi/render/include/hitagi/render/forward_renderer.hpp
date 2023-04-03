#pragma once
#include <hitagi/render/renderer.hpp>
#include <hitagi/render/utils.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gfx/render_graph.hpp>

namespace hitagi::render {
struct ColorPass {
public:
    gfx::ResourceHandle depth;
    gfx::ResourceHandle color;

private:
    friend class ForwardRenderer;
    gfx::ResourceHandle                                            bindless_info_buffer;
    gfx::ResourceHandle                                            frame_constant;
    gfx::ResourceHandle                                            instance_constant;
    std::pmr::unordered_map<asset::Material*, gfx::ResourceHandle> material_constants;
};

class ForwardRenderer : public IRenderer {
public:
    ForwardRenderer(const Application& app, gfx::Device::Type gfx_device_type, gui::GuiManager* gui_manager = nullptr);
    ~ForwardRenderer() override;

    void Tick() override;

    auto RenderScene(const asset::Scene& scene, const asset::CameraNode& camera, std::optional<gfx::ViewPort> viewport = std::nullopt, std::optional<math::vec2u> texture_size = std::nullopt) -> gfx::ResourceHandle override;
    auto RenderGui(std::optional<gfx::ResourceHandle> target = std::nullopt) -> gfx::ResourceHandle override;
    auto GetFrameTime() const noexcept -> std::chrono::duration<double> override { return m_Clock.DeltaTime(); }

    inline auto GetColorPass() const noexcept { return m_ColorPass; }

private:
    const Application& m_App;

    core::Clock   m_Clock;
    std::uint64_t m_FrameIndex = 0;

    std::unique_ptr<gfx::Device>    m_GfxDevice;
    std::shared_ptr<gfx::SwapChain> m_SwapChain;
    gfx::RenderGraph                m_RenderGraph;
    gfx::ResourceHandle             m_SwapChianHandle;

    std::unique_ptr<GuiRenderUtils> m_GuiRenderUtils;

    ColorPass m_ColorPass;
};

}  // namespace hitagi::render