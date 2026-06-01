module;

#include <tracy/Tracy.hpp>

module render;
import std;

namespace hitagi::render {

RenderRuntime::RenderRuntime(gfx::Device& device, const Application& app, std::string_view name)
    : RuntimeModule(std::format("RenderRuntime{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_SwapChain(device.CreateSwapChain({
          .name        = "swapchain",
          .window      = app.GetWindow(),
          .clear_color = math::Color(0, 0, 0, 1),
      })),
      m_RenderGraph(m_GfxDevice, "RenderRuntimeGraph"),
      m_GuiRenderUtils(std::make_unique<GuiRenderUtils>(m_GfxDevice)),
      m_TextRenderUtils(std::make_unique<TextRenderUtils>(m_GfxDevice, app.GetConfig().asset_root_path / "fonts")) {
    m_Clock.Start();
}

void RenderRuntime::Tick() {
    ZoneScopedN("RenderRuntimeFrame");

    if (m_App.WindowSizeChanged()) {
        m_SwapChain->Resize();
    }

    if (m_RenderGraph.IsValid(m_GuiTarget) && m_GuiDrawData != nullptr) {
        m_GuiRenderUtils->GuiPass(m_RenderGraph, m_GuiTarget, *m_GuiDrawData, m_ClearGuiTarget);
    }

    if (m_RenderGraph.Compile()) {
        m_RenderGraph.Execute();
    }

    m_SwapChain->Present();
    m_Clock.Tick();
    m_GuiTarget      = {};
    m_GuiDrawData    = nullptr;
    m_ClearGuiTarget = false;
}

auto RenderRuntime::MakeContext() noexcept -> RenderContext {
    return RenderContext{
        .device     = m_GfxDevice,
        .graph      = m_RenderGraph,
        .swap_chain = m_SwapChain,
    };
}

void RenderRuntime::RenderGui(rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target) {
    m_GuiTarget      = target;
    m_GuiDrawData    = &draw_data;
    m_ClearGuiTarget = clear_target;
}

void RenderRuntime::RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target) {
    m_TextRenderUtils->TextPass(m_RenderGraph, target, commands, clear_target);
}

void RenderRuntime::CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer, gfx::TextureSubresourceLayer to_layer) {
    m_RenderGraph.QueueTextureExtraction(from, std::move(to), from_layer, to_layer);
}

void RenderRuntime::CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer) {
    m_RenderGraph.QueueBufferExtraction(from, std::move(to), from_layer);
}

void RenderRuntime::ToSwapChain(rg::TextureHandle from) {
    auto context = MakeContext();
    m_PresentPass.Build(context, from);
}

}  // namespace hitagi::render
