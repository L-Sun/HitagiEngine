#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/renderable.hpp>

#include <imgui.h>

#include <list>
#include <queue>

namespace hitagi::gui {
class GuiManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    template <typename DrawFunc>
    inline void DrawGui(DrawFunc&& draw_func) {
        m_GuiDrawTasks.emplace([func = std::forward<DrawFunc>(draw_func)] { func(); });
    }

private:
    std::shared_ptr<resource::Texture> LoadFontTexture();
    void                               MouseEvent();
    void                               KeysEvent();

    std::pmr::vector<resource::Renderable> PrepareImGuiRenderables();

    core::Clock                       m_Clock;
    std::queue<std::function<void()>> m_GuiDrawTasks;

    std::shared_ptr<resource::MaterialInstance> m_ImGuiMaterial;
    std::shared_ptr<resource::VertexArray>      m_Vertices;
    std::shared_ptr<resource::IndexArray>       m_Indices;
};

}  // namespace hitagi::gui

namespace hitagi {
extern std::unique_ptr<gui::GuiManager> g_GuiManager;
}
