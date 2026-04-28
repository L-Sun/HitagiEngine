module;

#include <spdlog/logger.h>

export module engine;
export import std;
export import utils;
export import core;
export import math;
export import physics;
export import gfx;
export import ecs;
export import render;
export import gui;
export import debugger;
export import app;
export import hid;
export import asset;

export namespace hitagi {
class Engine : public core::RuntimeModule {
public:
    Engine(const std::filesystem::path& config_path = "hitagi.json");
    static auto Get() -> Engine* { return static_cast<Engine*>(core::RuntimeModule::GetModule("Engine")); }

    void Tick() final;

    auto AddSubModule(std::unique_ptr<core::RuntimeModule> module, core::RuntimeModule* after = nullptr) -> core::RuntimeModule* final;
    auto SetRenderer(std::unique_ptr<render::IRenderer> renderer) -> render::IRenderer*;

    inline auto& App() const noexcept { return *m_App; };
    inline auto& Renderer() const noexcept { return *m_Renderer; };
    inline auto& GuiManager() const noexcept { return *m_GuiManager; }
    inline auto& Physics() const noexcept { return *m_PhysicsWorld; }

    inline auto GetDeltaTime() const noexcept { return m_Clock.DeltaTime(); }

private:
    std::uint64_t m_FrameIndex = 0;
    core::Clock   m_Clock;

    Application*           m_App          = nullptr;
    core::RuntimeModule*   m_OutLogicArea = nullptr;
    physics::PhysicsWorld* m_PhysicsWorld = nullptr;
    render::IRenderer*     m_Renderer     = nullptr;
    gui::GuiManager*       m_GuiManager   = nullptr;
};

}  // namespace hitagi
