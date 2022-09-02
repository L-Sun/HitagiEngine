#include <hitagi/engine.hpp>
#include <hitagi/application.hpp>

#include "my_game.hpp"

#include <iostream>

#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine;

    if (!engine.Initialize()) {
        std::cout << "Engine Initialize failed, will exit now." << std::endl;
        return -1;
    }

    engine.LoadModule(std::make_unique<MyGame>());

#ifdef _DEBUG
    try {
#endif
        while (!hitagi::app->IsQuit()) {
            engine.Tick();
        }
#ifdef _DEBUG
    } catch (std::exception ex) {
        std::cout << ex.what() << std::endl;
    }
#endif

    engine.Finalize();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
