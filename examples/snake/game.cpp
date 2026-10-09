
import interop.spdlog;
import std;
import engine;
import snake_demo;

auto main() -> int {
#ifdef HITAGI_DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    hitagi::Engine engine(hitagi::AppConfig{
        .title       = "Hitagi Snake",
        .width       = 960,
        .height      = 720,
        .gfx_backend = "Vulkan",
    });

    engine.AddSubModule(std::make_unique<hitagi::snake::SnakeGameModule>(engine));

    while (!engine.App().IsQuit()) {
        engine.Tick();
    }

    return 0;
}
