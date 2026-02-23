module;

#include <imgui.h>
#include <spdlog/logger.h>

export module gui;
import std;
import core;
import gfx;
import hid;
import app;

export namespace hitagi::gui {
class GuiManager final : public RuntimeModule {
public:
    GuiManager(Application& application);
    ~GuiManager() final;
    void Tick() final;

    template <typename DrawFunc>
    inline void DrawGui(DrawFunc&& draw_func) {
        m_GuiDrawTasks.emplace([func = std::forward<DrawFunc>(draw_func)] { func(); });
    }

    auto        ReadTexture(rg::TextureHandle texture) -> ImTextureID;
    inline auto PopReadTextures() noexcept { return std::move(m_ReadTextures); }

private:
    void LoadFont();
    void MouseEvent();
    void KeysEvent();

    Application&       m_App;
    hid::InputManager& m_InputManager;
    core::Clock        m_Clock;

    std::queue<std::function<void()>, std::pmr::deque<std::function<void()>>> m_GuiDrawTasks;

    std::pmr::vector<rg::TextureHandle> m_ReadTextures;
};

}  // namespace hitagi::gui
