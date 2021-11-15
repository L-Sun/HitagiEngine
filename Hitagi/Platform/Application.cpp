#include "Application.hpp"

#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "GraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"
#include "AssetManager.hpp"
#include "GameLogic.hpp"
#include "IPhysicsManager.hpp"
#include "ThreadManager.hpp"

using namespace Hitagi;

bool Application::m_Quit = false;

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
    if ((ret = g_FileIOManager->Initialize()) != 0) return ret;
    if ((ret = g_AssetManager->Initialize()) != 0) return ret;
    if ((ret = g_InputManager->Initialize()) != 0) return ret;
    if ((ret = g_SceneManager->Initialize()) != 0) return ret;
    if ((ret = g_PhysicsManager->Initialize()) != 0) return ret;
    if ((ret = g_GraphicsManager->Initialize()) != 0) return ret;
    if ((ret = g_GameLogic->Initialize()) != 0) return ret;
    if ((ret = m_Clock.Initialize()) != 0) return ret;

    m_Clock.Start();
    return ret;
}

// Finalize all sub modules and clean up all runtime temporary files.
void Application::Finalize() {
    g_GameLogic->Finalize();
    g_GraphicsManager->Finalize();
    g_PhysicsManager->Finalize();
    g_SceneManager->Finalize();
    g_InputManager->Finalize();
    g_AssetManager->Finalize();
    g_FileIOManager->Finalize();
    g_DebugManager->Finalize();
    g_MemoryManager->Finalize();
    g_ThreadManager->Finalize();

    m_Clock.Finalize();
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

// One cycle of the main loop
void Application::Tick() {
    m_Clock.Tick();
    g_ThreadManager->Tick();
    g_MemoryManager->Tick();
    g_DebugManager->Tick();
    g_FileIOManager->Tick();
    g_AssetManager->Tick();
    g_InputManager->Tick();
    g_GameLogic->Tick();
    g_SceneManager->Tick();
    g_PhysicsManager->Tick();

    // -------------Before Render-------------------
    g_GraphicsManager->Tick();
    // -------------After Render--------------------

    m_FPS = 1.0 / m_Clock.deltaTime().count();
    if (m_FPSLimit != -1) {
        // std::this_thread::sleep_until(m_Clock.GetBaseTime() + m_FrameIndex * std::chrono::milliseconds(1000) / m_FPSLimit);
    }
    m_FrameIndex++;
}

void Application::SetCommandLineParameters(int argc, char** argv) {
    m_ArgSize = argc;
    m_Arg     = argv;
}

GfxConfiguration& Application::GetConfiguration() { return m_Config; }

bool Application::IsQuit() { return m_Quit; }
