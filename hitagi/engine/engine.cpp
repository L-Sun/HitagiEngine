module;

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

module engine;
import std;
import magic_enum;
import physics;
import gfx;
import render;
import gui;
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

class OutLogicArea : public core::RuntimeModule {
public:
    OutLogicArea() : core::RuntimeModule("OutLogicArea") {}
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
    m_Device       = add_inner_module(gfx::create_device(magic_enum::enum_cast<gfx::Device::Type>(m_App->GetConfig().gfx_backend).value()));
    auto* graphics_services = add_inner_module(std::make_unique<GraphicsServices>(*m_Device));
    m_Queues                = &graphics_services->queues;
    m_Bindings              = graphics_services->bindings.get();
    m_ShaderCompiler        = &graphics_services->compiler;
    m_AssetManager = add_inner_module(std::make_unique<asset::AssetManager>(*m_FileIO, *m_JobSystem, m_App->GetConfig().asset_root_path));
    m_PhysicsWorld = add_inner_module(std::make_unique<physics::PhysicsWorld>(*m_JobSystem));

    // Game or editor logic here
    m_OutLogicArea = add_inner_module(std::make_unique<OutLogicArea>());

    // use modified state -> Render
    add_inner_module(std::make_unique<debugger::DebugManager>());
    m_Renderer      = add_inner_module(std::make_unique<render::DefaultRenderer>(*m_Device, *m_Queues, *m_Bindings, *m_ShaderCompiler, *m_FileIO, *m_App));
    m_GuiManager    = add_inner_module(std::make_unique<gui::GuiManager>(*m_App, *m_FileIO));
    m_RenderRuntime = static_cast<render::RenderRuntime*>(add_inner_module(std::make_unique<render::RenderRuntime>(*m_Device, *m_Queues, *m_Bindings, *m_ShaderCompiler, *m_App)));

    m_Clock.Start();
}

Engine::~Engine() {
    UnloadAllSubModules();
}

void Engine::Tick() {
    ZoneScopedN("Engine");
    core::RuntimeModule::Tick();
    m_Clock.Tick();

    static bool tracy_plot_configured = false;
    if (!tracy_plot_configured) {
        TracyPlotConfig("CPU Frame Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        tracy_plot_configured = true;
    }
    TracyPlot("CPU Frame Time (ms)", m_Clock.DeltaTime().count() * 1000.0);
    FrameMark;
}

auto Engine::SetRenderer(std::unique_ptr<render::IRenderer> renderer) -> render::IRenderer* {
    if (renderer == nullptr) {
        m_Logger->warn("Can not set a empty renderer!");
        return nullptr;
    }

    m_Renderer = static_cast<render::IRenderer*>(core::RuntimeModule::AddSubModule(std::move(renderer), m_Renderer));
    return m_Renderer;
}

auto Engine::AddSubModule(std::unique_ptr<core::RuntimeModule> module, core::RuntimeModule* after) -> core::RuntimeModule* {
    return m_OutLogicArea->AddSubModule(std::move(module), after);
}

}  // namespace hitagi
