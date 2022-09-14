#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/graphics/render_graph.hpp>

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

    struct GuiRenderPass {
        gfx::ResourceHandle output;
    };

    GuiRenderPass Render(gfx::RenderGraph* render_graph);

private:
    void LoadFontTexture();
    void InitRenderPipeline();
    void MouseEvent();
    void KeysEvent();
    void UpdateMeshData();

    core::Clock m_Clock;

    std::queue<std::function<void()>, std::pmr::deque<std::function<void()>>> m_GuiDrawTasks;

    gfx::PipelineState m_Pipeline;
    struct DrawData {
        resource::Mesh                       mesh_data;
        std::pmr::vector<math::vec4u>        scissor_rects;
        std::pmr::vector<resource::Texture*> textures;
    } m_DrawData;
    std::shared_ptr<resource::Texture> m_FontTexture;
};

}  // namespace hitagi::gui

namespace hitagi {
extern gui::GuiManager* gui_manager;
}
