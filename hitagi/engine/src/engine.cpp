#include <hitagi/engine.hpp>
#include <hitagi/render/forward_renderer.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

using namespace std::literals;

namespace hitagi {
Engine::Engine(const std::filesystem::path& config_path) : RuntimeModule("Engine") {
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    m_Logger->info("Initialize Engine");

    auto add_inner_module = [&]<typename T>(std::unique_ptr<T> module) -> T* {
        return static_cast<T*>(AddSubModule(std::unique_ptr<RuntimeModule>{module.release()}));
    };

    memory_manager  = add_inner_module(std::make_unique<core::MemoryManager>());
    thread_manager  = add_inner_module(std::make_unique<core::ThreadManager>());
    file_io_manager = add_inner_module(std::make_unique<core::FileIOManager>());
    m_App           = add_inner_module(Application::CreateApp(config_path));
    m_GuiManager    = add_inner_module(std::make_unique<gui::GuiManager>(*m_App));
    m_Renderer      = add_inner_module(std::make_unique<render::ForwardRenderer>(*m_App, gfx::Device::Type::DX12, m_GuiManager));
    asset_manager   = add_inner_module(std::make_unique<asset::AssetManager>(m_App->GetConfig().asset_root_path));
    debug_manager   = add_inner_module(std::make_unique<debugger::DebugManager>());
}

void Engine::Tick() {
    ZoneScopedN("Engine");
    RuntimeModule::Tick();
    FrameMark;
}

void Engine::SetRenderer(std::unique_ptr<render::IRenderer> renderer) {
    if (renderer == nullptr) {
        m_Logger->warn("Can not set a empty renderer!");
        return;
    }
    auto old_renderer_iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](const auto& mod) {
        return mod.get() == m_Renderer;
    });

    *old_renderer_iter = std::unique_ptr<RuntimeModule>{renderer.release()};
    m_Renderer         = static_cast<render::IRenderer*>(old_renderer_iter->get());
}

}  // namespace hitagi