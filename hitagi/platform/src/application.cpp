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

#include <thread>

using namespace hitagi;

bool Application::sm_Quit = false;

// Parse command line, read configuration, initialize all sub modules
int Application::Initialize() {
    m_Logger = spdlog::stdout_color_mt("Application");

    int ret = 0;

    m_Logger->info("Initialize Moudules...");
    if ((ret = g_ThreadManager->Initialize()) != 0) return ret;
    if ((ret = g_MemoryManager->Initialize()) != 0) return ret;
    if ((ret = g_DebugManager->Initialize()) != 0) return ret;
    if ((ret = g_FileIoManager->Initialize()) != 0) return ret;
    if ((ret = g_ConfigManager->Initialize()) != 0) return ret;
    if ((ret = g_AssetManager->Initialize()) != 0) return ret;
    if ((ret = g_InputManager->Initialize()) != 0) return ret;
    if ((ret = g_GuiManager->Initialize()) != 0) return ret;
    if ((ret = g_SceneManager->Initialize()) != 0) return ret;
    if ((ret = g_GraphicsManager->Initialize()) != 0) return ret;
    if ((ret = g_GamePlay->Initialize()) != 0) return ret;

    if (ret == 0) m_Initialized = true;
    return ret;
}

// Finalize all sub modules and clean up all runtime temporary files.
void Application::Finalize() {
    g_GamePlay->Finalize();
    g_GraphicsManager->Finalize();
    g_SceneManager->Finalize();
    g_GuiManager->Finalize();
    g_InputManager->Finalize();
    g_AssetManager->Finalize();
    g_ConfigManager->Finalize();
    g_FileIoManager->Finalize();
    g_DebugManager->Finalize();
    g_MemoryManager->Finalize();
    g_ThreadManager->Finalize();

    m_Initialized = false;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

// One cycle of the main loop
void Application::Tick() {
    g_ThreadManager->Tick();
    g_MemoryManager->Tick();
    g_DebugManager->Tick();
    g_FileIoManager->Tick();
    g_ConfigManager->Tick();
    g_AssetManager->Tick();
    g_InputManager->Tick();
    g_GuiManager->Tick();
    g_GamePlay->Tick();
    g_SceneManager->Tick();

    // -------------Before Render-------------------
    g_GraphicsManager->Tick();
    // -------------After Render--------------------
}

void Application::SetCommandLineParameters(int argc, char** argv) {
    m_ArgSize = argc;
    m_Arg     = argv;
}

bool Application::IsQuit() { return sm_Quit; }
