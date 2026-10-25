export module engine;
export import :frame_stages;
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
class Engine final {
public:
    Engine(AppConfig config = {});
    ~Engine();

    void Tick();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;
    auto GetWorld() noexcept -> ecs::World& { return *m_World; }

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
    // Declaration order expresses ownership only; Schedule expresses execution.
    std::unique_ptr<core::MemoryManager> m_MemoryManager;
    std::unique_ptr<core::FileIOManager> m_FileIO;
    std::unique_ptr<core::JobSystem> m_JobSystem;
    std::unique_ptr<Application> m_App;
    std::unique_ptr<gfx::Device> m_Device;
    std::unique_ptr<gfx::CommandQueues> m_Queues;
    std::unique_ptr<gfx::BindlessUtils> m_Bindings;
    std::unique_ptr<gfx::ShaderCompiler> m_ShaderCompiler;
    std::unique_ptr<asset::AssetManager> m_AssetManager;
    std::unique_ptr<physics::PhysicsWorld> m_PhysicsWorld;
    core::JobSystem m_FrameJobs{core::JobSystem::Execution::CallingThread};
    core::Clock m_Clock;
    // System state and cached tasks are released before the borrowed services.
    std::unique_ptr<ecs::World> m_World;
};

}  // namespace hitagi
