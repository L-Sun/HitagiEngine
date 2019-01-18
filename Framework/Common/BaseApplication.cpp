#include "BaseApplication.hpp"
#include <iostream>

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
}

void BaseApplication::SetCommandLineParameters(int argc, char** argv) {
    m_nArgC  = argc;
    m_ppArgV = argv;
}

bool BaseApplication::IsQuit() { return m_bQuit; }
