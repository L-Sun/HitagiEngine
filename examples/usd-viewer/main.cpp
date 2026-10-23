import std;
import engine;
import viewer;

int main(int argc, char** argv) {
    try {
        auto options = hitagi::examples::ParseOptions(argc, argv);
        if (options.help) {
            hitagi::examples::PrintUsage();
            return 0;
        }

        hitagi::Engine engine(std::move(options.app));
        auto           viewer = std::make_unique<hitagi::examples::USDViewer>(
            engine.App(), engine.ResourceLoadContext(), engine.FileIO(), engine.Jobs(), options.scene, options.renderer);
        engine.AddSubModule(std::move(viewer));

        hitagi::examples::PrintUsage();
        std::uint64_t frames = 0;
        while (!engine.App().IsQuit() && (options.frames == 0 || frames < options.frames)) {
            engine.Tick();
            ++frames;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "USD Viewer: " << error.what() << '\n';
        return 1;
    }
}
