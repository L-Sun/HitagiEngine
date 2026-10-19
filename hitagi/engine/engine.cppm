export module engine;
import std;
export import std;
export import utils;
export import core;
export import math;
export import physics;
export import gfx;
export import ecs;
export import gui;
export import debugger;
export import app;
export import hid;
export import asset;

export namespace hitagi {
class Engine : public core::RuntimeModule {
public:
    Engine(AppConfig config = {});
    ~Engine() override;

    void Tick() final;

    // Composition root accessors: the engine owns every subsystem below and hands
    // them to game/editor modules explicitly instead of through a global registry.
    inline auto& FileIO() const noexcept { return *m_FileIO; }
    inline auto& Jobs() const noexcept { return *m_JobSystem; }
    inline auto& App() const noexcept { return *m_App; };
    inline auto& Device() const noexcept { return *m_Device; }
    inline auto& Queues() const noexcept { return *m_Queues; }
    inline auto& Bindings() const noexcept { return *m_Bindings; }
    inline auto& ShaderCompiler() const noexcept { return *m_ShaderCompiler; }
    inline auto  ResourceLoadContext() const noexcept -> asset::ResourceLoadContext { return {.device = *m_Device, .queues = *m_Queues, .bindings = *m_Bindings, .shader_compiler = *m_ShaderCompiler}; }
    inline auto& Assets() const noexcept { return *m_AssetManager; }
    inline auto& Physics() const noexcept { return *m_PhysicsWorld; }

    inline auto GetDeltaTime() const noexcept { return m_Clock.DeltaTime(); }

private:
    void TickFrame();
    bool m_InFrame = false;
    // Infrastructure services are members rather than sub-modules: they never
    // tick, and member order states their lifetime. ~Engine() unloads the module
    // tree first, so every sub-module is gone before these are destroyed.
    std::unique_ptr<core::MemoryManager> m_MemoryManager;
    std::unique_ptr<core::FileIOManager> m_FileIO;
    std::unique_ptr<core::JobSystem>     m_JobSystem;

    std::uint64_t m_FrameIndex = 0;
    core::Clock   m_Clock;

    // Sub-modules, owned by the RuntimeModule tree in construction order.
    Application*           m_App            = nullptr;
    gfx::Device*           m_Device         = nullptr;
    gfx::CommandQueues*    m_Queues         = nullptr;
    gfx::BindlessUtils*    m_Bindings       = nullptr;
    gfx::ShaderCompiler*   m_ShaderCompiler = nullptr;
    asset::AssetManager*   m_AssetManager   = nullptr;
    physics::PhysicsWorld* m_PhysicsWorld   = nullptr;
};

}  // namespace hitagi
