#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/gfx/render_graph.hpp>

#include <imgui.h>

#include <functional>
#include <list>
#include <queue>

namespace hitagi::gui {
class GuiManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "GuiManager"; }

    template <typename DrawFunc>
    inline void DrawGui(DrawFunc&& draw_func) {
        m_GuiDrawTasks.emplace([func = std::forward<DrawFunc>(draw_func)] { func(); });
    }

    auto ReadTexture(gfx::ResourceHandle tex) -> gfx::ResourceHandle;
    auto GuiRenderPass(gfx::RenderGraph& rg, gfx::ResourceHandle tex) -> gfx::ResourceHandle;

private:
    void LoadFont();
    void InitFontTexture(gfx::Device& gfx_device);
    void InitRenderPipeline(gfx::Device& gfx_device);
    void MouseEvent();
    void KeysEvent();

    core::Clock m_Clock;

    std::queue<std::function<void()>, std::pmr::deque<std::function<void()>>> m_GuiDrawTasks;

    struct GraphicsData {
        std::shared_ptr<gfx::RenderPipeline> pipeline;
        std::shared_ptr<gfx::GpuBuffer>      vertices_buffer;
        std::shared_ptr<gfx::GpuBuffer>      indices_buffer;
        std::shared_ptr<gfx::Texture>        font_texture;
        // TODO render graph
        std::shared_ptr<gfx::GpuBuffer> upload_heap;
    } m_GfxData;

    std::pmr::vector<gfx::ResourceHandle> m_ReadTextures;
};

}  // namespace hitagi::gui

namespace hitagi {
extern gui::GuiManager* gui_manager;
}
