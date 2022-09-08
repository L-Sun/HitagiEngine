#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/graphics/draw_data.hpp>

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

    const gfx::GuiDrawData& GetDrawData();

private:
    void LoadFontTexture();
    void MouseEvent();
    void KeysEvent();

    core::Clock m_Clock;

    std::queue<std::function<void()>, std::pmr::deque<std::function<void()>>> m_GuiDrawTasks;

    gfx::GuiDrawData                   m_DrawData;
    std::shared_ptr<resource::Texture> m_FontTexture;
};

}  // namespace hitagi::gui

namespace hitagi {
extern gui::GuiManager* gui_manager;
}
