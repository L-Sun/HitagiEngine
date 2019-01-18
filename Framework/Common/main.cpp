#include <iostream>
#include "BaseApplication.hpp"

using namespace My;

int main(int argc, char** argv) {
    int ret;

    g_pApp->SetCommandLineParameters(argc, argv);

    if ((ret = g_pApp->Initialize()) != 0) {
        std::cout << "App Initialize failed, will exit now." << std::endl;
        return ret;
    }

    while (!g_pApp->IsQuit()) {
        g_pApp->Tick();
    }

    g_pApp->Finalize();

    return 0;
}