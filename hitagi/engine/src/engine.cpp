#include <hitagi/engine.hpp>
#include <hitagi/core/core.hpp>
#include <hitagi/application.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
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
        || !(asset_manager    = add_inner_module(std::make_unique<resource::AssetManager>()))   
        || !(app              = add_inner_module(Application::CreateApp()))                     
        || !(input_manager    = add_inner_module(std::make_unique<hid::InputManager>()))        
        || !(graphics_manager = add_inner_module(std::make_unique<gfx::GraphicsManager>()))
        || !(scene_manager    = add_inner_module(std::make_unique<resource::SceneManager>()))   
        || !(debug_manager    = add_inner_module(std::make_unique<debugger::DebugManager>()))   
        || !(gui_manager      = add_inner_module(std::make_unique<gui::GuiManager>()))
    ) {
        Finalize();
        return false;
    }
    // clang-format on

    return true;
}

void Engine::Finalize() {
    // make sure all pmr resources allcated in module are released before the memory manager finalization
    auto _memory_manager_module = std::move(m_SubModules.front());
    m_SubModules.pop_front();

    RuntimeModule::Finalize();

    _memory_manager_module->Finalize();
    _memory_manager_module = nullptr;

    m_Logger->info("Finalized");
    m_Logger = nullptr;
}

}  // namespace hitagi