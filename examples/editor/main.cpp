#include "editor.hpp"

#include <spdlog/spdlog.h>
#include <vcruntime_new_debug.h>
#include <tracy/Tracy.hpp>

auto main(int argc, char** argv) -> int {
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    hitagi::Engine engine;

    engine.AddSubModule(std::make_unique<hitagi::Editor>(engine));

    while (!engine.App().IsQuit()) {
        engine.Tick();
    }

    return 0;
}
