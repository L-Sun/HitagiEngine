#include <hitagi/engine.hpp>
#include <hitagi/core/core.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/application.hpp>
#include "hitagi/core/config_manager.hpp"

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
        return static_cast<T*>(m_InnerModules.emplace_back(std::unique_ptr<RuntimeModule>{module.release()}).get());
    };

    memory_manager   = add_inner_module(std::make_unique<core::MemoryManager>());
    thread_manager   = add_inner_module(std::make_unique<core::ThreadManager>());
    file_io_manager  = add_inner_module(std::make_unique<core::FileIOManager>());
    config_manager   = add_inner_module(std::make_unique<core::ConfigManager>());
    app              = add_inner_module(Application::CreateApp());
    asset_manager    = add_inner_module(std::make_unique<resource::AssetManager>());
    scene_manager    = add_inner_module(std::make_unique<resource::SceneManager>());
    input_manager    = add_inner_module(std::make_unique<hid::InputManager>());
    gui_manager      = add_inner_module(std::make_unique<gui::GuiManager>());
    debug_manager    = add_inner_module(std::make_unique<debugger::DebugManager>());
    graphics_manager = add_inner_module(std::make_unique<graphics::GraphicsManager>());

    std::pmr::list<RuntimeModule*> intialized_modules = {};
    for (auto& inner_module : m_InnerModules) {
        if (inner_module->Initialize()) {
            intialized_modules.emplace_back(inner_module.get());
        } else {
            while (!intialized_modules.empty()) {
                intialized_modules.back()->Finalize();
                intialized_modules.pop_back();
            }
            return false;
        }
    }

    // it will use pmr
    m_CreatedModules = std::pmr::set<std::unique_ptr<RuntimeModule>>{};
    m_OutterModules  = std::pmr::set<RuntimeModule*>{};

    return true;
}

void Engine::Tick() {
    for (auto& inner_module : m_InnerModules) {
        inner_module->Tick();
    }

    for (auto outter_module : m_OutterModules) {
        outter_module->Tick();
    }
}

void Engine::Finalize() {
    auto memory_manager = std::move(m_InnerModules.front());

    for (auto iter = m_OutterModules.rbegin(); iter != m_OutterModules.rend(); iter++) {
        auto outter_module = *iter;
        outter_module->Finalize();
    }

    for (auto iter = m_InnerModules.rbegin(); iter != m_InnerModules.rend(); iter++) {
        auto inner_module = iter->get();
        if (inner_module != nullptr) inner_module->Finalize();
    }

    m_OutterModules.clear();
    m_CreatedModules.clear();
    m_InnerModules.clear();
    memory_manager->Finalize();
}

bool Engine::LoadModule(RuntimeModule* module, bool force) {
    assert(module != nullptr);
    if (module == nullptr) {
        m_Logger->warn("You can not add nullptr module! This operation is ommited!");
        return false;
    }
    if (
        std::find_if(m_InnerModules.begin(), m_InnerModules.end(),
                     [&](auto& _m) { return _m->GetName() == module->GetName(); }) != m_InnerModules.end()) {
        m_Logger->warn("The name of module can not be ({})!. This operation is ommited!", module->GetName());
        return false;
    }
    if (std::find(m_OutterModules.begin(), m_OutterModules.end(), module) != m_OutterModules.end()) {
        m_Logger->warn("You are repeatly load a module ({})! This operation is ommited!", module->GetName());
        return false;
    }
    if (!force &&
        std::find_if(m_OutterModules.begin(), m_OutterModules.end(),
                     [&](RuntimeModule* _m) { return _m->GetName() == module->GetName(); }) != m_OutterModules.end()) {
        m_Logger->warn("There are a same name module ({})! This operation is ommited!", module->GetName());
        return false;
    }
    module->Initialize();
    m_OutterModules.emplace(module);
    return true;
}

void Engine::UnloadModule(RuntimeModule* module) {
    if (auto iter = std::find(m_OutterModules.begin(), m_OutterModules.end(), module); iter == m_OutterModules.end()) {
        m_Logger->warn("You are unload an unexisted module ({})! This operation is ommited!", module->GetName());
        return;
    } else {
        m_OutterModules.erase(iter);
    }

    if (auto iter = std::find_if(m_CreatedModules.begin(), m_CreatedModules.end(), [module](const auto& _m) {
            return _m.get() == module;
        });
        iter != m_CreatedModules.end()) {
        m_CreatedModules.erase(iter);
    }
}

}  // namespace hitagi