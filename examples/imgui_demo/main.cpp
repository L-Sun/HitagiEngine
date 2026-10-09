import interop.imgui;

import engine;

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine(hitagi::AppConfig{
        .gfx_backend = "Vulkan",
    });

    while (!engine.App().IsQuit()) {
        engine.GuiManager().DrawGui([]() {
            static bool open = true;
            ImGui::ShowDemoWindow(&open);
        });

        auto& render_runtime = engine.RenderRuntime();
        auto render_target = render_runtime.GetRenderGraph().Create(
            hitagi::gfx::TextureDesc{
                .width       = render_runtime.GetSwapChain().GetWidth(),
                .height      = render_runtime.GetSwapChain().GetHeight(),
                .format      = hitagi::gfx::Format::R8G8B8A8_UNORM,
                .clear_value = hitagi::math::Color::Black(),
                .usages      = hitagi::gfx::TextureUsageFlags::RenderTarget | hitagi::gfx::TextureUsageFlags::CopySrc,
            });
        render_runtime.RenderGui(render_target, engine.GuiManager().GetDrawData(), true);
        render_runtime.ToSwapChain(render_target);
        engine.Tick();
    }

    return 0;
}
