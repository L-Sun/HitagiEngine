#include <hitagi/engine.hpp>

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine;

    while (!engine.App().IsQuit()) {
        engine.GuiManager().DrawGui([]() {
            static bool open = true;
            ImGui::ShowDemoWindow(&open);
        });
        engine.Renderer().RenderGui();
        engine.Tick();
    }

    return 0;
}
