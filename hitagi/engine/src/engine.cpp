#include <hitagi/engine.hpp>
#include <hitagi/render/forward_renderer.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

using namespace std::literals;

namespace hitagi {

class OutLogicArea : public RuntimeModule {
public:
    OutLogicArea() : RuntimeModule("OutLogicArea") {}
    inline static auto Get() {
        return static_cast<OutLogicArea*>(sm_AllModules.at("OutLogicArea"));
    }
};

Engine::Engine(const std::filesystem::path& config_path) : RuntimeModule("Engine") {
    auto add_inner_module = [&]<typename T>(std::unique_ptr<T> module) -> T* {
        return static_cast<T*>(RuntimeModule::AddSubModule(std::unique_ptr<RuntimeModule>{module.release()}));
    };

    add_inner_module(std::make_unique<core::MemoryManager>());
    add_inner_module(std::make_unique<core::FileIOManager>());
    add_inner_module(std::make_unique<core::ThreadManager>());

    // Input
    m_App = add_inner_module(Application::CreateApp(config_path));  // input manager is created here

    // update state
    auto device = add_inner_module(gfx::Device::Create(magic_enum::enum_cast<gfx::Device::Type>(m_App->GetConfig().gfx_backend).value()));
    add_inner_module(std::make_unique<asset::AssetManager>(m_App->GetConfig().asset_root_path));

    // Game or editor logic here
    add_inner_module(std::make_unique<OutLogicArea>());

    // use modified state -> Render
    add_inner_module(std::make_unique<debugger::DebugManager>());
    m_GuiManager = add_inner_module(std::make_unique<gui::GuiManager>(*m_App));
    m_Renderer   = add_inner_module(std::make_unique<render::ForwardRenderer>(*device, *m_App, m_GuiManager));
}

void Engine::Tick() {
    ZoneScopedN("Engine");
    RuntimeModule::Tick();
    FrameMark;
}

auto Engine::SetRenderer(std::unique_ptr<render::IRenderer> renderer) -> render::IRenderer* {
    if (renderer == nullptr) {
        m_Logger->warn("Can not set a empty renderer!");
        return nullptr;
    }

    m_SubModules.back() = std::unique_ptr<RuntimeModule>{renderer.release()};
    m_Renderer          = static_cast<render::IRenderer*>(m_SubModules.back().get());
    return m_Renderer;
}

auto Engine::AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after) -> RuntimeModule* {
    return OutLogicArea::Get()->AddSubModule(std::move(module), after);
}

}  // namespace hitagi