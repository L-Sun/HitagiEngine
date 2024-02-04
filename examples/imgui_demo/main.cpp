#include <hitagi/engine.hpp>

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine;

    while (!engine.App().IsQuit()) {
        engine.GuiManager().DrawGui([]() {
            static bool open = true;
            ImGui::ShowDemoWindow(&open);
        });

        auto render_target = engine.Renderer().GetRenderGraph().Create(
            hitagi::gfx::TextureDesc{
                .width       = engine.Renderer().GetSwapChain().GetWidth(),
                .height      = engine.Renderer().GetSwapChain().GetHeight(),
                .format      = hitagi::gfx::Format::R8G8B8A8_UNORM,
                .clear_value = hitagi::math::Color::Black(),
                .usages      = hitagi::gfx::TextureUsageFlags::RenderTarget | hitagi::gfx::TextureUsageFlags::CopySrc,
            });
        engine.Renderer().RenderGui(render_target, true);
        engine.Renderer().ToSwapChain(render_target);
        engine.Tick();
    }

    return 0;
}
