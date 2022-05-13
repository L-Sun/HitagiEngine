#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/texture.hpp>

#include <imgui.h>

#include <list>
#include <queue>

namespace hitagi::gui {
class GuiManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline ImDrawData* GetGuiDrawData() {
        return ImGui::GetDrawData();
    }
    inline auto GetGuiFontTexture() const noexcept { return m_FontTexture; }

    template <typename DrawFunc>
    inline void DrawGui(DrawFunc&& draw_func) {
        m_GuiDrawTasks.emplace([func = std::forward<DrawFunc>(draw_func)] { func(); });
    }

private:
    void LoadFontTexture();
    void MouseEvent();
    void KeysEvent();

    core::Clock                        m_Clock;
    std::shared_ptr<resource::Texture> m_FontTexture;
    std::list<core::Buffer>            m_FontsData;
    std::queue<std::function<void()>>  m_GuiDrawTasks;
};

}  // namespace hitagi::gui

namespace hitagi {
extern std::unique_ptr<gui::GuiManager> g_GuiManager;
}
