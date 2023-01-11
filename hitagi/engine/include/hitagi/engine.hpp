#pragma once
#include <hitagi/core/core.hpp>
#include <hitagi/application.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/render/renderer.hpp>

namespace hitagi {
class Engine : public RuntimeModule {
public:
    Engine(const std::filesystem::path& config_path = "hitagi.json");

    void Tick() final;

    void SetRenderer(std::unique_ptr<render::IRenderer> renderer);

    inline auto& App() const noexcept { return *m_App; };
    inline auto& Renderer() const noexcept { return *m_Renderer; };
    inline auto& GuiManager() const noexcept { return *m_GuiManager; }

private:
    std::uint64_t      m_FrameIndex = 0;
    Application*       m_App        = nullptr;
    render::IRenderer* m_Renderer   = nullptr;
    gui::GuiManager*   m_GuiManager = nullptr;
};

extern Engine* engine;

}  // namespace hitagi