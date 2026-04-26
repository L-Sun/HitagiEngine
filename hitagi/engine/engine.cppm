module;

#include <spdlog/logger.h>

export module engine;
export import std;
export import utils;
export import core;
export import math;
export import gfx;
export import ecs;
export import render;
export import gui;
export import debugger;
export import app;
export import hid;
export import asset;
export import physics;

export namespace hitagi {
class Engine : public RuntimeModule {
public:
    Engine(const std::filesystem::path& config_path = "hitagi.json");
    static auto Get() -> Engine* { return static_cast<Engine*>(RuntimeModule::GetModule("Engine")); }

    void Tick() final;

    auto AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after = nullptr) -> RuntimeModule* final;
    auto SetRenderer(std::unique_ptr<render::IRenderer> renderer) -> render::IRenderer*;

    inline auto& App() const noexcept { return *m_App; };
    inline auto& Renderer() const noexcept { return *m_Renderer; };
    inline auto& GuiManager() const noexcept { return *m_GuiManager; }

    inline auto GetDeltaTime() const noexcept { return m_Clock.DeltaTime(); }

private:
    std::uint64_t m_FrameIndex = 0;
    core::Clock   m_Clock;

    Application*       m_App = nullptr;
    RuntimeModule*     m_OutLogicArea;
    render::IRenderer* m_Renderer   = nullptr;
    gui::GuiManager*   m_GuiManager = nullptr;
};

}  // namespace hitagi
