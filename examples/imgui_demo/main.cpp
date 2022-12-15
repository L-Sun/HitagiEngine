#include <hitagi/engine.hpp>
#include <hitagi/application.hpp>

#include "imgui_demo.hpp"

#include <iostream>

#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine(hitagi::Application::CreateApp());

    engine.AddSubModule(std::make_unique<ImGuiDemo>(engine.GuiManager()));

#ifdef _DEBUG
    try {
#endif
        while (!engine.App().IsQuit()) {
            engine.Tick();
        }
#ifdef _DEBUG
    } catch (std::exception ex) {
        std::cout << ex.what() << std::endl;
    }
#endif

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
