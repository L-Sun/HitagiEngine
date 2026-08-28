#include <spdlog/spdlog.h>

import editor;
import engine;
import snake_demo;

auto main(int argc, char** argv) -> int {
#ifdef HITAGI_DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    auto options = hitagi::ParseEditorLaunchOptions(argc, argv);
    auto config  = hitagi::LoadEditorAppConfig();
    config.title = "Hitagi Snake Editor";

    hitagi::Engine engine(std::move(config));
    engine.AddSubModule(std::make_unique<hitagi::snake::SnakeEditorPanel>(engine));
    engine.AddSubModule(std::make_unique<hitagi::Editor>(engine, std::move(options)));

    while (!engine.App().IsQuit()) {
        engine.Tick();
    }

    hitagi::SaveEditorAppConfig(engine.App().GetConfig());
    return 0;
}
