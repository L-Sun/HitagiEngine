#include <iostream>
#include "BaseApplication.hpp"

using namespace My;

#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

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
#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
