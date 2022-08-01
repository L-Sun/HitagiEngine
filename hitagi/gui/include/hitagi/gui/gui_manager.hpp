#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/renderable.hpp>

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

    resource::MaterialInstance m_ImGuiMaterialInstance;
    resource::Mesh             m_ImGuiMesh;
};

}  // namespace hitagi::gui

namespace hitagi {
extern gui::GuiManager* gui_manager;
}
