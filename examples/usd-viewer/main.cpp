import std;
import engine;
import viewer;

int main(int argc, char** argv) {
    try {
        hitagi::core::Runtime runtime;
        auto                  options = hitagi::examples::ParseOptions(argc, argv);
        if (options.help) {
            hitagi::examples::PrintUsage();
            return 0;
        }
        hitagi::App app(runtime, "USDViewer");
        app.AddPlugin(hitagi::DefaultPlugins{
            .runtime  = runtime,
            .platform = options.app,
            .backend  = options.backend,
        });
        auto& world    = app.GetWorld();
        auto& graphics = world.GetResource<hitagi::gfx::GraphicsContext>();
        app.AddPlugin(hitagi::examples::USDViewerPlugin{
            .platform  = world.GetResource<hitagi::platform::PlatformHost>(),
            .resources = {.device = *graphics.device, .queues = graphics.queues, .bindings = *graphics.bindings, .shader_compiler = graphics.compiler},
            .files     = runtime.IO(),
            .jobs      = runtime.Jobs(),
            .scene     = options.scene,
            .renderer  = options.renderer,
        });
        hitagi::examples::PrintUsage();
        return app.Run(options.frames);
    } catch (const std::exception& error) {
        std::cerr << "USD Viewer: " << error.what() << '\n';
        return 1;
    }
}
