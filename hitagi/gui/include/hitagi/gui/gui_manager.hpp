#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gfx/render_graph.hpp>

#include <imgui.h>

#include <functional>
#include <list>
#include <queue>

namespace hitagi::gui {
class GuiManager final : public RuntimeModule {
public:
    GuiManager(Application& application);
    ~GuiManager() final;
    void Tick() final;

    void Render();

    template <typename DrawFunc>
    inline void DrawGui(DrawFunc&& draw_func) {
        m_GuiDrawTasks.emplace([func = std::forward<DrawFunc>(draw_func)] { func(); });
    }

    auto        ReadTexture(gfx::ResourceHandle tex) -> gfx::ResourceHandle;
    inline auto PopReadTextures() noexcept { return std::move(m_ReadTextures); }

private:
    void LoadFont();
    void MouseEvent();
    void KeysEvent();

    Application& m_App;
    core::Clock  m_Clock;

    std::queue<std::function<void()>, std::pmr::deque<std::function<void()>>> m_GuiDrawTasks;

    std::pmr::vector<gfx::ResourceHandle> m_ReadTextures;
};

}  // namespace hitagi::gui
