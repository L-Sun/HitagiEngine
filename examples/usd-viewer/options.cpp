export module viewer:options;

import std;
import app;

export namespace hitagi::examples {

enum class RendererType { Pbr, Toon, Deferred };

auto RendererName(RendererType type) -> std::string_view {
    switch (type) {
        case RendererType::Pbr: return "pbr";
        case RendererType::Toon: return "toon";
        case RendererType::Deferred: return "deferred";
    }
    throw std::invalid_argument("Invalid renderer");
}

struct ViewerOptions {
    AppConfig             app{.title = "USD Viewer", .width = 1600, .height = 1000};
    std::filesystem::path scene    = "examples/usd-viewer/assets/studio.usda";
    RendererType          renderer = RendererType::Deferred;
    std::uint64_t         frames   = 0;
    bool                  help     = false;
};

auto ParseOptions(int argc, char** argv) -> ViewerOptions {
    ViewerOptions options;
    bool          has_scene = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--headless") {
            options.app.headless = true;
        } else if (argument.starts_with("--renderer=")) {
            const auto name = argument.substr(11);
            if (name == "pbr")
                options.renderer = RendererType::Pbr;
            else if (name == "toon")
                options.renderer = RendererType::Toon;
            else if (name == "deferred")
                options.renderer = RendererType::Deferred;
            else
                throw std::invalid_argument("--renderer must be pbr, toon or deferred");
        } else if (argument.starts_with("--backend=")) {
            const auto name = argument.substr(10);
            if (name != "Vulkan" && name != "DX12")
                throw std::invalid_argument("--backend must be Vulkan or DX12");
            options.app.gfx_backend = name;
        } else if (argument.starts_with("--frames=")) {
            const auto value = argument.substr(9);
            const auto [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), options.frames);
            if (error != std::errc{} || end != value.data() + value.size() || options.frames == 0)
                throw std::invalid_argument("--frames must be a positive integer");
        } else if (!argument.starts_with('-') && !has_scene) {
            options.scene = std::filesystem::path(argument);
            has_scene     = true;
        } else {
            throw std::invalid_argument(std::format("Unknown argument: {}", argument));
        }
    }
    return options;
}

void PrintUsage() {
    std::println("Usage: xmake r usd-viewer [scene.usd] [--renderer=pbr|toon|deferred]");
    std::println("       [--backend=Vulkan|DX12] [--headless] [--frames=N]");
    std::println(
        "Tab: switch renderer | LMB: orbit | MMB: pan | Wheel: zoom | F: frame | Esc: clear selection");
}

}  // namespace hitagi::examples
