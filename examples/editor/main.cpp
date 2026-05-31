#include <spdlog/spdlog.h>
#include <vcruntime_new_debug.h>
#include <tracy/Tracy.hpp>

import engine;
import editor;

auto main(int argc, char** argv) -> int {
#ifdef HITAGI_DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    auto options     = hitagi::ParseEditorLaunchOptions(argc, argv);
    auto config_path = hitagi::GetEditorConfigPath();
    auto app_config  = hitagi::LoadEditorAppConfig(config_path);

    hitagi::Engine engine(std::move(app_config));

    engine.AddSubModule(std::make_unique<hitagi::Editor>(engine, std::move(options)));

    while (!engine.App().IsQuit()) {
        engine.Tick();
    }

    hitagi::SaveEditorAppConfig(engine.App().GetConfig(), config_path);

    return 0;
}
