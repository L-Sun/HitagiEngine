#include "BaseApplication.hpp"
#include <iostream>
#include <thread>

using namespace My;
using namespace std;

bool BaseApplication::m_bQuit = false;

BaseApplication::BaseApplication(GfxConfiguration& cfg) : m_Config(cfg) {}

// Parse command line, read configuration, initialize all sub modules
int BaseApplication::Initialize() {
    int ret = 0;

    cout << m_Config;

    cerr << "Initialize Memory Manager: ";
    if ((ret = g_pMemoryManager->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize Asset Loader: ";
    if ((ret = g_pAssetLoader->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize Scene Manager: ";
    if ((ret = g_pSceneManager->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize Graphics Manager: ";
    if ((ret = g_pGraphicsManager->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize Input Manager: ";
    if ((ret = g_pInputManager->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize Physics Manager: ";
    if ((ret = g_pPhysicsManager->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize GameLogic Manager: ";
    if ((ret = g_pGameLogic->Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    cerr << "Success" << endl;

    cerr << "Initialize Timer: ";
    if ((ret = m_clock.Initialize()) != 0) {
        cerr << "Failed. err = " << ret;
        return ret;
    }
    m_clock.Start();

#ifdef DEBUG
    cerr << "Initialize Debug Manager: ";
    if ((ret = g_pDebugManager->Initialize()) != 0) {
        cerr << "Failed. err =" << ret;
        return ret;
    }
    cerr << "Success" << endl;

#endif

    return ret;
}

// Finalize all sub modules and clean up all runtime temporary files.
void BaseApplication::Finalize() {
#ifdef DEBUG
    g_pDebugManager->Finalize();
#endif
    g_pGameLogic->Finalize();
    g_pGraphicsManager->Finalize();
    g_pPhysicsManager->Finalize();
    g_pInputManager->Finalize();
    g_pSceneManager->Finalize();
    g_pAssetLoader->Finalize();
    g_pMemoryManager->Finalize();
}

// One cycle of the main loop
void BaseApplication::Tick() {
    m_clock.Tick();
    g_pMemoryManager->Tick();
    g_pAssetLoader->Tick();
    g_pSceneManager->Tick();
    g_pInputManager->Tick();
    g_pPhysicsManager->Tick();
    g_pGraphicsManager->Tick();
    g_pGameLogic->Tick();
#ifdef DEBUG
    g_pDebugManager->Tick();
#endif
    if (m_frame_counter != -1) {
        m_sumFPS += 1.0 / m_clock.deltaTime().count();
    }
    m_frame_counter++;
    if (m_frame_counter == 30) {
        m_FPS           = m_sumFPS / 30;
        m_sumFPS        = 0;
        m_frame_counter = 0;
    }
    m_k += m_FPS - 60;
    this_thread::sleep_until(m_clock.tickTime() + chrono::seconds(1) / 60.0 +
                             m_k * chrono::microseconds(1));
}

void BaseApplication::SetCommandLineParameters(int argc, char** argv) {
    m_nArgC  = argc;
    m_ppArgV = argv;
}

bool BaseApplication::IsQuit() { return m_bQuit; }
