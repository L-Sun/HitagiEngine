#include "BaseApplication.hpp"

#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "GraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"
#include "ResourceManager.hpp"
#include "GameLogic.hpp"
#include "IPhysicsManager.hpp"
#include "ThreadManager.hpp"

using namespace Hitagi;

bool BaseApplication::m_Quit = false;

BaseApplication::BaseApplication(GfxConfiguration& cfg) : m_Config(cfg) {
    m_Logger = spdlog::stdout_color_mt("Application");
}

// Parse command line, read configuration, initialize all sub modules
int BaseApplication::Initialize() {
    int ret = 0;

    m_Logger->info("Initialize Moudules...");
    if ((ret = g_ThreadManager->Initialize()) != 0) return ret;
    if ((ret = g_MemoryManager->Initialize()) != 0) return ret;
    if ((ret = g_FileIOManager->Initialize()) != 0) return ret;
    if ((ret = g_InputManager->Initialize()) != 0) return ret;
    if ((ret = g_ResourceManager->Initialize()) != 0) return ret;
    if ((ret = g_SceneManager->Initialize()) != 0) return ret;
    if ((ret = g_GraphicsManager->Initialize()) != 0) return ret;
    if ((ret = g_PhysicsManager->Initialize()) != 0) return ret;
    if ((ret = g_GameLogic->Initialize()) != 0) return ret;
    if ((ret = m_Clock.Initialize()) != 0) return ret;
    if ((ret = g_DebugManager->Initialize()) != 0) return ret;

    m_Clock.Start();
    return ret;
}

// Finalize all sub modules and clean up all runtime temporary files.
void BaseApplication::Finalize() {
    g_DebugManager->Finalize();
    g_GameLogic->Finalize();
    g_GraphicsManager->Finalize();
    g_PhysicsManager->Finalize();
    g_SceneManager->Finalize();
    g_ResourceManager->Finalize();
    g_InputManager->Finalize();
    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();
    g_ThreadManager->Finalize();

    m_Clock.Finalize();
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

// One cycle of the main loop
void BaseApplication::Tick() {
    m_Clock.Tick();
    g_ThreadManager->Tick();
    g_MemoryManager->Tick();
    g_FileIOManager->Tick();
    g_InputManager->Tick();
    g_ResourceManager->Tick();
    g_SceneManager->Tick();
    g_PhysicsManager->Tick();
    g_GraphicsManager->Tick();
    g_GameLogic->Tick();
#if defined(_DEBUG)
    g_DebugManager->Tick();
#endif

    m_FPS = 1.0 / m_Clock.deltaTime().count();
    if (m_FPSLimit != -1) {
        std::this_thread::sleep_for(std::chrono::seconds(1) / static_cast<double>(m_FPSLimit));
    }
}

void BaseApplication::SetCommandLineParameters(int argc, char** argv) {
    m_ArgSize = argc;
    m_Arg     = argv;
}

bool BaseApplication::IsQuit() { return m_Quit; }
