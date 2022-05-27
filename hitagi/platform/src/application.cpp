#include <hitagi/application.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/gameplay.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <ranges>
#include <thread>

using namespace hitagi;

bool Application::sm_Quit = false;

// Parse command line, read configuration, initialize all sub modules
int Application::Initialize() {
    m_Logger = spdlog::stdout_color_mt("Application");

    int ret = 0;
    m_Logger->info("Initialize Moudules...");
    // Core
    if ((ret = m_Modules.emplace_back(g_ThreadManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_MemoryManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_FileIoManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_ConfigManager.get())->Initialize()) != 0)
        return ret;

    // Resource
    if ((ret = m_Modules.emplace_back(g_AssetManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_SceneManager.get())->Initialize()) != 0)
        return ret;

    // Windows
    InitializeWindows();

    if ((ret = m_Modules.emplace_back(g_InputManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_GraphicsManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_GuiManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_DebugManager.get())->Initialize()) != 0)
        return ret;
    if ((ret = m_Modules.emplace_back(g_GamePlay.get())->Initialize()) != 0)
        return ret;

    if (ret == 0) m_Initialized = true;
    return ret;
}

// Finalize all sub modules and clean up all runtime temporary files.
void Application::Finalize() {
    for (auto iter = m_Modules.rbegin(); iter != m_Modules.rend(); iter++) {
        auto _module = *iter;
        _module->Finalize();
    }

    g_GamePlay        = nullptr;
    g_DebugManager    = nullptr;
    g_GuiManager      = nullptr;
    g_SceneManager    = nullptr;
    g_GraphicsManager = nullptr;
    g_InputManager    = nullptr;
    g_AssetManager    = nullptr;
    g_ConfigManager   = nullptr;
    g_FileIoManager   = nullptr;
    g_MemoryManager   = nullptr;
    g_ThreadManager   = nullptr;

    m_Initialized = false;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

// One cycle of the main loop
void Application::Tick() {
    g_ThreadManager->Tick();
    g_MemoryManager->Tick();
    g_FileIoManager->Tick();
    g_ConfigManager->Tick();
    g_AssetManager->Tick();
    g_InputManager->Tick();
    g_GuiManager->Tick();
    g_GamePlay->Tick();
    g_SceneManager->Tick();
    g_DebugManager->Tick();

    // -------------Before Render-------------------
    g_GraphicsManager->Tick();
    // -------------After Render--------------------
}

void Application::SetCommandLineParameters(int argc, char** argv) {
    m_ArgSize = argc;
    m_Arg     = argv;
}

bool Application::IsQuit() { return sm_Quit; }
