#pragma once
#include "IRuntimeModule.hpp"
#include "Timer.hpp"
#include "Image.hpp"

#include <imgui.h>

#include <list>
#include <queue>

namespace Hitagi::Gui {
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
    void MapKey();

    Core::Clock                   m_Clock;
    std::shared_ptr<Asset::Image> m_FontTexture;
    std::list<Core::Buffer>       m_FontsData;

    std::queue<std::function<void()>> m_GuiDrawTasks;
};

}  // namespace Hitagi::Gui

namespace Hitagi {
extern std::unique_ptr<Gui::GuiManager> g_GuiManager;
}
