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

class OutLogicArea : public core::RuntimeModule {
public:
    OutLogicArea() : core::RuntimeModule("OutLogicArea") {}
    inline static auto Get() {
        return static_cast<OutLogicArea*>(core::RuntimeModule::GetModule("OutLogicArea"));
    }
};

Engine::Engine(const std::filesystem::path& config_path) : core::RuntimeModule("Engine") {
    tracy::SetThreadName("Hitagi/Main");

    auto add_inner_module = [&]<typename T>(std::unique_ptr<T> module) -> T* {
        return static_cast<T*>(core::RuntimeModule::AddSubModule(std::unique_ptr<core::RuntimeModule>{module.release()}));
    };

    add_inner_module(std::make_unique<core::MemoryManager>());
    add_inner_module(std::make_unique<core::FileIOManager>());
    add_inner_module(std::make_unique<core::JobSystem>());

    // Input
    m_App = add_inner_module(Application::CreateApp(config_path));  // input manager is created here

    // update state
    auto device = add_inner_module(gfx::create_device(magic_enum::enum_cast<gfx::Device::Type>(m_App->GetConfig().gfx_backend).value()));
    add_inner_module(std::make_unique<asset::AssetManager>(m_App->GetConfig().asset_root_path));
    m_PhysicsWorld = add_inner_module(std::make_unique<physics::PhysicsWorld>());

    // Game or editor logic here
    add_inner_module(std::make_unique<OutLogicArea>());

    // use modified state -> Render
    add_inner_module(std::make_unique<debugger::DebugManager>());
    m_Renderer   = add_inner_module(std::make_unique<render::ForwardRenderer>(*device, *m_App));
    m_GuiManager = add_inner_module(std::make_unique<gui::GuiManager>(*m_App));

    m_Clock.Start();
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

    m_SubModules.back() = std::unique_ptr<core::RuntimeModule>{renderer.release()};
    m_Renderer          = static_cast<render::IRenderer*>(m_SubModules.back().get());
    return m_Renderer;
}

auto Engine::AddSubModule(std::unique_ptr<core::RuntimeModule> module, core::RuntimeModule* after) -> core::RuntimeModule* {
    return OutLogicArea::Get()->AddSubModule(std::move(module), after);
}

}  // namespace hitagi
