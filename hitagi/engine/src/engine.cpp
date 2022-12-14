#include <hitagi/engine.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>

using namespace std::literals;

namespace hitagi {
Engine* engine = nullptr;

bool Engine::Initialize() {
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    m_Logger = spdlog::stdout_color_mt("Engine");
    m_Logger->info("Initialize Engine");

    auto add_inner_module = [&]<typename T>(std::unique_ptr<T> module) -> T* {
        return static_cast<T*>(LoadModule(std::unique_ptr<RuntimeModule>{module.release()}));
    };

    // clang-format off
    if (false
        || !(memory_manager   = add_inner_module(std::make_unique<core::MemoryManager>()))      
        || !(thread_manager   = add_inner_module(std::make_unique<core::ThreadManager>()))      
        || !(file_io_manager  = add_inner_module(std::make_unique<core::FileIOManager>()))      
        || !(config_manager   = add_inner_module(std::make_unique<core::ConfigManager>()))      
        || !(app              = add_inner_module(Application::CreateApp()))                     
        || !(graphics_manager = add_inner_module(std::make_unique<gfx::GraphicsManager>()))
        || !(asset_manager    = add_inner_module(std::make_unique<asset::AssetManager>()))   
        || !(debug_manager    = add_inner_module(std::make_unique<debugger::DebugManager>()))   
        || !(gui_manager      = add_inner_module(std::make_unique<gui::GuiManager>()))
    ) {
        Finalize();
        return false;
    }
    // clang-format on

    return true;
}

void Engine::Tick() {
    ZoneScopedN("Engine");
    RuntimeModule::Tick();
    FrameMark;
}

void Engine::Finalize() {
    // make sure all pmr resources allcated in module are released before the memory manager finalization
    auto _memory_manager_module = std::move(m_SubModules.front());
    m_SubModules.pop_front();

    RuntimeModule::Finalize();

    _memory_manager_module->Finalize();
    _memory_manager_module = nullptr;

    memory_manager   = nullptr;
    thread_manager   = nullptr;
    file_io_manager  = nullptr;
    config_manager   = nullptr;
    asset_manager    = nullptr;
    app              = nullptr;
    graphics_manager = nullptr;
    debug_manager    = nullptr;
    gui_manager      = nullptr;
}

}  // namespace hitagi