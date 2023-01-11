#pragma once
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/gfx/render_graph.hpp>

namespace hitagi::render {
class GuiRenderUtils {
public:
    GuiRenderUtils(gui::GuiManager& gui_manager, gfx::Device& gfx_device);

    auto GuiPass(gfx::RenderGraph& render_graph, gfx::ResourceHandle target) -> gfx::ResourceHandle;

protected:
    gui::GuiManager& m_GuiManager;

    // GUI render data
    struct GuiRenderData {
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        std::shared_ptr<gfx::GpuBuffer>      vertices_buffer;
        std::shared_ptr<gfx::GpuBuffer>      indices_buffer;
        std::shared_ptr<gfx::Texture>        font_texture;
        std::shared_ptr<gfx::GpuBuffer>      upload_heap;
    } m_GfxData;

    gfx::ResourceHandle m_FontTextureHandle;
};

}  // namespace hitagi::render