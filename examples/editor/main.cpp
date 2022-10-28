#include "editor.hpp"
#include <tracy/Tracy.hpp>

#ifdef _WIN32
#ifdef _DEBUG
#include <crtdbg.h>
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

auto main(int argc, char** argv) -> int {
    auto engine    = std::make_unique<hitagi::Engine>();
    hitagi::engine = engine.get();

    if (!engine->Initialize()) {
        std::cout << "Engine Initialize failed, will exit now." << std::endl;
        return -1;
    }

    engine->LoadModule(std::make_unique<hitagi::Editor>());

#ifdef _DEBUG
    try {
#endif
        ZoneScopedN("Run");
        while (!hitagi::app->IsQuit()) {
            engine->Tick();
        }
#ifdef _DEBUG
    } catch (std::exception ex) {
        std::cout << ex.what() << std::endl;
    }
#endif

    engine->Finalize();
    hitagi::engine = nullptr;

#ifdef _WIN32
#ifdef _DEBUG
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
#endif
    return 0;
}
