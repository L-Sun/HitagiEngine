#include <iostream>
#include "IApplication.hpp"
#include "GraphicsManager.hpp"
#include "MemoryManager.hpp"

using namespace std;
using namespace My;

namespace My {
extern IApplication*    g_pApp;
extern MemoryManager*   g_pMemoryManager;
extern GraphicsManager* g_pGraphicsManager;
}  // namespace My

int main(int argc, char const* argv[]) {
    int ret;

    if ((ret = g_pApp->Initialize()) != 0) {
        cout << "App Initialize failed, will exit now." << endl;
        return ret;
    }

    if ((ret = g_pMemoryManager->Initialize()) != 0) {
        cout << "Memory Manager Initialize failed, will exit now." << endl;
        return ret;
    }

    if ((ret = g_pGraphicsManager->Initialize()) != 0) {
        cout << "Graphics Manager Initialize failed, will exit now." << endl;
        return ret;
    }

    while (!g_pApp->IsQuit()) {
        g_pApp->Tick();
        g_pMemoryManager->Tick();
        g_pGraphicsManager->Tick();
    }

    g_pGraphicsManager->Finalize();
    g_pMemoryManager->Finalize();
    g_pApp->Finalize();

    return 0;
}
