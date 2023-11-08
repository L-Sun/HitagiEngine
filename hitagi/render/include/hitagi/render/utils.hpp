#pragma once
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/render_graph/render_graph.hpp>

namespace hitagi::render {
class GuiRenderUtils {
public:
    GuiRenderUtils(gui::GuiManager& gui_manager, gfx::Device& gfx_device);

    void GuiPass(rg::RenderGraph& render_graph, rg::TextureHandle target, bool clear_target);

protected:
    gui::GuiManager& m_GuiManager;

    // GUI render data
    struct GuiRenderData {
        std::shared_ptr<gfx::Shader>         vs, ps;
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        std::shared_ptr<gfx::Texture>        font_texture;
        std::shared_ptr<gfx::Sampler>        sampler;
    } m_GfxData;

    rg::TextureHandle m_FontTexture;
};

}  // namespace hitagi::render