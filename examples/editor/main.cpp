#include "editor.hpp"

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

#ifdef _WIN32
#ifdef _DEBUG
#include <crtdbg.h>
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine;

    engine.AddSubModule(std::make_unique<hitagi::Editor>(engine));

    while (!engine.App().IsQuit()) {
        engine.Tick();
    }

#ifdef _WIN32
#ifdef _DEBUG
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
#endif
    return 0;
}
