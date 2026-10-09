module;

#include "interop/tracy_macros.hpp"

export module render:runtime;
import interop.tracy;

import std;
import utils;
import math;
import core;
import gfx;
import asset;
import gui;
import app;

import :types;
import :gui;
import :text;
import :present;

export namespace hitagi::render {

class RenderRuntime : public core::RuntimeModule {
public:
    RenderRuntime(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, const Application& app, std::string_view name = "");

    void Tick() override;

    auto MakeContext() noexcept -> RenderContext;

    void RenderGui(rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target);

    void RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target = false);

    void CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer = {}, gfx::TextureSubresourceLayer to_layer = {});
    void CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer = {});

    void ToSwapChain(rg::TextureHandle from);

    inline auto GetFrameTime() const noexcept -> std::chrono::duration<double> { return m_Clock.DeltaTime(); }

    inline auto GetRenderGraph() noexcept -> rg::RenderGraph& { return m_RenderGraph; }

    auto GetSwapChain() const noexcept -> gfx::SwapChain& { return *m_SwapChain; }

    auto GetSwapChainPtr() const noexcept -> std::shared_ptr<gfx::SwapChain> { return m_SwapChain; }

private:
    const Application&               m_App;
    gfx::Device&                     m_GfxDevice;
    gfx::CommandQueues&              m_Queues;
    gfx::BindlessUtils&              m_Bindings;
    const gfx::ShaderCompiler&       m_ShaderCompiler;
    std::shared_ptr<gfx::SwapChain>  m_SwapChain;
    rg::RenderGraph                  m_RenderGraph;
    std::unique_ptr<GuiRenderUtils>  m_GuiRenderUtils;
    std::unique_ptr<TextRenderUtils> m_TextRenderUtils;
    passes::Present                  m_PresentPass;
    rg::TextureHandle                m_GuiTarget;
    const gui::GuiDrawData*          m_GuiDrawData    = nullptr;
    bool                             m_ClearGuiTarget = false;
    core::Clock                      m_Clock;
};

}  // namespace hitagi::render

namespace hitagi::render {

RenderRuntime::RenderRuntime(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, const Application& app, std::string_view name)
    : RuntimeModule(std::format("RenderRuntime{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_Queues(queues),
      m_Bindings(bindings),
      m_ShaderCompiler(compiler),
      m_SwapChain(hitagi::gfx::SwapChain::Create(device, queues.Get(hitagi::gfx::CommandType::Graphics), {
                                                                                                             .name        = "swapchain",
                                                                                                             .window      = app.GetWindow(),
                                                                                                             .clear_color = math::Color(0, 0, 0, 1),
                                                                                                         })),
      m_RenderGraph(m_GfxDevice, queues, bindings, "RenderRuntimeGraph"),
      m_GuiRenderUtils(std::make_unique<GuiRenderUtils>(m_GfxDevice, bindings, compiler)),
      m_TextRenderUtils(std::make_unique<TextRenderUtils>(m_GfxDevice, queues, bindings, compiler, app.GetConfig().asset_root_path / "fonts")) {
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
