module;

#include "interop/tracy_macros.hpp"

module engine;
import interop.tracy;
import interop.magic_enum;

import std;
import physics;
import gfx;
import debugger;
import app;
import asset;

using namespace std::literals;

namespace hitagi {

// Own infrastructure before consumers; resources receive the individual services.
class GraphicsServices final : public core::RuntimeModule {
public:
    explicit GraphicsServices(gfx::Device& device)
        : RuntimeModule("GraphicsServices"), queues(device), bindings(gfx::BindlessUtils::Create(device)), compiler("Engine") {}
    ~GraphicsServices() final { queues.WaitIdle(); }
    void Tick() final { queues.NewFrame(); }

    gfx::CommandQueues                  queues;
    std::unique_ptr<gfx::BindlessUtils> bindings;
    gfx::ShaderCompiler                 compiler;
};

Engine::Engine(AppConfig config)
    : core::RuntimeModule("Engine"),
      m_MemoryManager(std::make_unique<core::MemoryManager>()),
      m_FileIO(std::make_unique<core::FileIOManager>()),
      m_JobSystem(std::make_unique<core::JobSystem>()) {
#ifdef TRACY_ENABLE
    tracy::SetThreadName("Hitagi/Main");
#endif

    auto add_inner_module = [&]<typename T>(std::unique_ptr<T> module) -> T* {
        return static_cast<T*>(core::RuntimeModule::AddSubModule(std::unique_ptr<core::RuntimeModule>{module.release()}));
    };

    // Sub-modules are constructed in dependency order and destroyed in reverse,
    // so every reference handed down here stays valid for the receiver's lifetime.

    // Input
    m_App = add_inner_module(Application::CreateApp(std::move(config)));  // input manager is created here

    // update state
    m_Device                = add_inner_module(gfx::create_device(magic_enum::enum_cast<gfx::Device::Type>(m_App->GetConfig().gfx_backend).value()));
    auto* graphics_services = add_inner_module(std::make_unique<GraphicsServices>(*m_Device));
    m_Queues                = &graphics_services->queues;
    m_Bindings              = graphics_services->bindings.get();
    m_ShaderCompiler        = &graphics_services->compiler;
    m_AssetManager          = add_inner_module(std::make_unique<asset::AssetManager>(*m_FileIO, *m_JobSystem, m_App->GetConfig().asset_root_path));
    m_PhysicsWorld          = add_inner_module(std::make_unique<physics::PhysicsWorld>(*m_JobSystem));

    // Application modules are appended after infrastructure in their chosen order.
    add_inner_module(std::make_unique<debugger::DebugManager>());

    m_Clock.Start();
    m_App->SetModalFrameCallback([this] { TickFrame(); });
}

Engine::~Engine() {
    m_App->SetModalFrameCallback({});
    UnloadAllSubModules();
}

void Engine::Tick() {
    m_App->Tick();
    TickFrame();
}

void Engine::TickFrame() {
    ZoneScopedN("Engine");
    if (m_InFrame) return;
    m_InFrame = true;
    struct FrameGuard {
        bool& active;
        ~FrameGuard() { active = false; }
    } guard{m_InFrame};
    // The application is already pumping events, possibly inside the Windows
    // modal sizing loop. Never re-enter that pump or an in-progress render graph.
    for (const auto& module : m_SubModules) {
        if (module.get() == m_App) continue;
        ZoneScoped;
        ZoneName(module->GetName().data(), module->GetName().size());
        module->Tick();
    }
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
