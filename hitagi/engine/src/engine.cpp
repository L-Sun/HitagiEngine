#include <hitagi/engine.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

using namespace std::literals;

namespace hitagi {
Engine::Engine(std::unique_ptr<Application> application) : RuntimeModule("Engine") {
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    m_Logger->info("Initialize Engine");

    auto add_inner_module = [&]<typename T>(std::unique_ptr<T> module) -> T* {
        return static_cast<T*>(AddSubModule(std::unique_ptr<RuntimeModule>{module.release()}));
    };

    memory_manager   = add_inner_module(std::make_unique<core::MemoryManager>());
    thread_manager   = add_inner_module(std::make_unique<core::ThreadManager>());
    file_io_manager  = add_inner_module(std::make_unique<core::FileIOManager>());
    app              = add_inner_module(std::move(application));
    graphics_manager = add_inner_module(std::make_unique<gfx::GraphicsManager>());
    asset_manager    = add_inner_module(std::make_unique<asset::AssetManager>(app->GetConfig().asset_root_path));
    debug_manager    = add_inner_module(std::make_unique<debugger::DebugManager>());
    gui_manager      = add_inner_module(std::make_unique<gui::GuiManager>());
    // clang-format on
}

void Engine::Tick() {
    ZoneScopedN("Engine");
    RuntimeModule::Tick();
    FrameMark;
}

}  // namespace hitagi