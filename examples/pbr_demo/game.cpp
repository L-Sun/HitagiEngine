#include <spdlog/spdlog.h>

import engine;
import pbr_demo_game;
import std;

namespace {

struct PbrDemoGameOptions {
    std::filesystem::path        cooked_scene = game::pbr_demo::DefaultCookedScenePath();
    std::optional<std::uint64_t> frames;
};

auto ParsePbrDemoGameOptions(int argc, char** argv) -> PbrDemoGameOptions {
    PbrDemoGameOptions options;
    for (int i = 1; i < argc; ++i) {
        const auto arg = std::string_view(argv[i]);
        if (arg == "--frames" && i + 1 < argc) {
            options.frames = std::stoull(argv[++i]);
        } else if (!arg.starts_with("--")) {
            options.cooked_scene = argv[i];
        }
    }
    return options;
}

}  // namespace

auto main(int argc, char** argv) -> int {
#ifdef HITAGI_DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    const auto options = ParsePbrDemoGameOptions(argc, argv);

    hitagi::Engine engine(hitagi::AppConfig{
        .title       = "Hitagi PBR Demo",
        .width       = 1280,
        .height      = 720,
        .gfx_backend = "Vulkan",
    });
    engine.SetRenderer(std::make_unique<game::pbr_demo::PbrDemoRenderer>(engine.ResourceLoadContext()));
    engine.AddSubModule(std::make_unique<game::pbr_demo::PbrDemoGame>(engine, options.cooked_scene));

    std::uint64_t frame_index = 0;
    while (!engine.App().IsQuit()) {
        engine.Tick();
        if (options.frames && ++frame_index >= *options.frames) break;
    }

    return 0;
}
