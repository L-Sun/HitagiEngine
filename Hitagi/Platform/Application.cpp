#include "Application.hpp"

#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "GraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "InputManager.hpp"
#include "GuiManager.hpp"
#include "DebugManager.hpp"
#include "AssetManager.hpp"
#include "GameLogic.hpp"
#include "IPhysicsManager.hpp"
#include "ThreadManager.hpp"

using namespace hitagi;

bool Application::sm_Quit = false;

Application::Application(GfxConfiguration& cfg) : m_Config(cfg) {
    m_Logger = spdlog::stdout_color_mt("Application");
#if defined(_DEBUG)
    spdlog::set_level(spdlog::level::debug);
#endif  // _DEBUG
}

// Parse command line, read configuration, initialize all sub modules
int Application::Initialize() {
    int ret = 0;

    m_Logger->info("Initialize Moudules...");
    if ((ret = g_ThreadManager->Initialize()) != 0) return ret;
    if ((ret = g_MemoryManager->Initialize()) != 0) return ret;
    if ((ret = g_DebugManager->Initialize()) != 0) return ret;
    if ((ret = g_FileIoManager->Initialize()) != 0) return ret;
    if ((ret = g_AssetManager->Initialize()) != 0) return ret;
    if ((ret = g_InputManager->Initialize()) != 0) return ret;
    if ((ret = g_GuiManager->Initialize()) != 0) return ret;
    if ((ret = g_SceneManager->Initialize()) != 0) return ret;
    if ((ret = g_PhysicsManager->Initialize()) != 0) return ret;
    if ((ret = g_GraphicsManager->Initialize()) != 0) return ret;
    if ((ret = g_GameLogic->Initialize()) != 0) return ret;

    if (ret == 0) m_Initialized = true;
    return ret;
}

// Finalize all sub modules and clean up all runtime temporary files.
void Application::Finalize() {
    g_GameLogic->Finalize();
    g_GraphicsManager->Finalize();
    g_PhysicsManager->Finalize();
    g_SceneManager->Finalize();
    g_GuiManager->Finalize();
    g_InputManager->Finalize();
    g_AssetManager->Finalize();
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
    OnResize();

    g_DebugManager->Profiler("ThreadManager", []() { g_ThreadManager->Tick(); });
    g_DebugManager->Profiler("MemoryManager", []() { g_MemoryManager->Tick(); });
    g_DebugManager->Profiler("DebugManager", []() { g_DebugManager->Tick(); });
    g_DebugManager->Profiler("FileIoManager", []() { g_FileIoManager->Tick(); });
    g_DebugManager->Profiler("AssetManager", []() { g_AssetManager->Tick(); });
    g_DebugManager->Profiler("InputManager", []() { g_InputManager->Tick(); });
    g_DebugManager->Profiler("GuiManager", []() { g_GuiManager->Tick(); });
    g_DebugManager->Profiler("GameLogic", []() { g_GameLogic->Tick(); });
    g_DebugManager->Profiler("SceneManager", []() { g_SceneManager->Tick(); });
    g_DebugManager->Profiler("PhysicsManager", []() { g_PhysicsManager->Tick(); });

    // -------------Before Render-------------------
    g_DebugManager->Profiler("GraphicsManager", []() { g_GraphicsManager->Tick(); });
    // -------------After Render--------------------
}

void Application::SetCommandLineParameters(int argc, char** argv) {
    m_ArgSize = argc;
    m_Arg     = argv;
}

GfxConfiguration& Application::GetConfiguration() { return m_Config; }

void Application::OnResize() {
    uint32_t width  = m_Rect.right - m_Rect.left;
    uint32_t height = m_Rect.bottom - m_Rect.top;

    if (m_Config.screen_width == width && m_Config.screen_height == height) {
        m_SizeChanged = false;
        return;
    }

    m_Config.screen_width  = width;
    m_Config.screen_height = height;

    m_SizeChanged = true;
}

bool Application::IsQuit() { return sm_Quit; }
