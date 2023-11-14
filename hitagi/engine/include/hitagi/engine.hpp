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

    auto AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after = nullptr) -> RuntimeModule* final;
    auto SetRenderer(std::unique_ptr<render::IRenderer> renderer) -> render::IRenderer*;

    inline auto& App() const noexcept { return *m_App; };
    inline auto& Renderer() const noexcept { return *m_Renderer; };
    inline auto& GuiManager() const noexcept { return *m_GuiManager; }

private:
    std::uint64_t      m_FrameIndex = 0;
    Application*       m_App        = nullptr;
    RuntimeModule*     m_OutLogicArea;
    render::IRenderer* m_Renderer   = nullptr;
    gui::GuiManager*   m_GuiManager = nullptr;
};

}  // namespace hitagi