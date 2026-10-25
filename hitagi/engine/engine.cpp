module;

#include "interop/tracy_macros.hpp"

module engine;
import interop.tracy;
import interop.magic_enum;
#ifdef __APPLE__
import interop.metal;
#endif

import std;
import physics;
import gfx;
import debugger;
import app;
import asset;

using namespace std::literals;

namespace hitagi {

Engine::Engine(AppConfig config)
    : m_MemoryManager(std::make_unique<core::MemoryManager>()),
      m_FileIO(std::make_unique<core::FileIOManager>()),
      m_JobSystem(std::make_unique<core::JobSystem>()),
      m_App(Application::CreateApp(std::move(config))),
      m_Device(gfx::create_device(magic_enum::enum_cast<gfx::Device::Type>(m_App->GetConfig().gfx_backend).value())),
      m_Queues(std::make_unique<gfx::CommandQueues>(*m_Device)),
      m_Bindings(gfx::BindlessUtils::Create(*m_Device)),
      m_ShaderCompiler(std::make_unique<gfx::ShaderCompiler>("Engine")),
      m_AssetManager(std::make_unique<asset::AssetManager>(*m_FileIO, *m_JobSystem, m_App->GetConfig().asset_root_path)),
      m_PhysicsWorld(std::make_unique<physics::PhysicsWorld>(*m_JobSystem)),
      m_World(std::make_unique<ecs::World>("Engine", ConfigureFrameStages)) {
#ifdef TRACY_ENABLE
    tracy::SetThreadName("Hitagi/Main");
#endif
    m_World->RegisterSystem<gfx::GraphicsFrameSystem>(*m_Device, *m_Queues, FrameStage::First);
    m_World->RegisterSystem<debugger::DebugManager>(FrameStage::First);
    m_Clock.Start();
    m_App->SetModalFrameCallback([this] { TickFrame(); });
}

Engine::~Engine() {
    m_App->SetModalFrameCallback({});
    // No new frame can produce work. Finish CPU submissions before the GPU
    // barrier, while every system and service is still alive.
    m_JobSystem->WaitForAll();
    m_Queues->WaitIdle();
    m_World.reset();
}

void Engine::Tick() {
    if (m_InFrame) return;
    {
        ZoneScopedN("Application Event Pump");
        m_App->PollEvents();
    }
    TickFrame();
}

void Engine::TickFrame() {
    ZoneScopedN("Engine");
    if (m_InFrame) return;
#ifdef __APPLE__
    // The frame can run inside Cocoa's live-resize callback. A lexical pool
    // drains only this frame's objects and leaves the enclosing native pools intact.
    auto autorelease_pool = interop::ns::TransferPtr(interop::ns::AutoreleasePool::alloc()->init());
#endif
    m_InFrame = true;
    struct FrameGuard {
        bool& active;
        ~FrameGuard() { active = false; }
    } guard{m_InFrame};
    // The same ECS dependency graph runs on the caller: platform/GUI/GPU work
    // must not migrate to Taskflow workers. Scene worlds retain parallel Update.
    m_World->Update(m_FrameJobs, m_Clock.DeltaTime().count());

    m_Clock.Tick();

    static bool tracy_plot_configured = false;
    if (!tracy_plot_configured) {
        TracyPlotConfig("CPU Frame Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        tracy_plot_configured = true;
    }
    TracyPlot("CPU Frame Time (ms)", m_Clock.DeltaTime().count() * 1000.0);
    FrameMark;
}

}  // namespace hitagi
