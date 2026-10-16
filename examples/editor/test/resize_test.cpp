#include "interop/gtest_macros.hpp"
import interop.gtest;
#ifdef _WIN32
import interop.win32;
#endif
import std;
import engine;
import editor;
import preset.deferred;

#ifdef _WIN32
namespace hitagi::tests {
void CheckEditorModalResize(std::string_view backend) {
    Engine engine({.title = "Editor resize regression", .width = 1800, .height = 1000, .gfx_backend = std::pmr::string(backend), .log_level = "warn", .headless = true});
    engine.SetRenderer(std::make_unique<render::DeferredRenderer>(
        engine.Device(), engine.Queues(), engine.Bindings(), engine.ShaderCompiler(), engine.FileIO(), engine.App()));
    engine.AddSubModule(std::make_unique<Editor>(engine));
    engine.Tick();
    const auto window = static_cast<HWND>(engine.App().GetWindow().ptr);
    const auto dpi    = std::max(engine.App().GetDpiRatio(), 1.0f);
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    for (const auto size : {std::pair{1500u, 900u}, std::pair{1900u, 700u}, std::pair{1600u, 1050u}}) {
        engine.App().ResizeWindow(size.first, size.second);
        // No Engine::Tick call: the native modal-loop timer must produce the frame.
        SendMessageW(window, WM_TIMER, 1, 0);
        const auto& data = engine.GuiManager().GetDrawData();
        EXPECT_EQ(data.display_size.x, size.first);
        EXPECT_EQ(data.display_size.y, size.second);
        EXPECT_EQ(engine.RenderRuntime().GetSwapChain().GetWidth(), size.first);
        EXPECT_EQ(engine.RenderRuntime().GetSwapChain().GetHeight(), size.second);
        EXPECT_NEAR(data.draw_lists.front().vertices[2].position.x, std::round(size.first / dpi) * dpi, 0.01f);
        EXPECT_EQ(data.draw_lists.size(), 2u);
        if (data.draw_lists.size() != 2) continue;
        const auto& scene = data.draw_lists[1].vertices;
        const float w = std::round(size.first / dpi), h = std::round(size.second / dpi);
        const float left   = w < 760 ? 0 : std::clamp(232.0f, 180.0f, w * 0.28f);
        const float right  = w < 580 ? 0 : std::clamp(296.0f, 232.0f, w * 0.35f);
        const float assets = std::clamp(220.0f, 112.0f, std::max(112.0f, h * 0.42f));
        // First resized frame must use the current UI layout, not stale rectangles.
        EXPECT_NEAR(scene[0].position.x, (left ? left + 4 : 0) * dpi, 0.01f);
        EXPECT_NEAR(scene[0].position.y, 84 * dpi, 0.01f);
        EXPECT_NEAR(scene[2].position.x, (w - right - (right ? 4 : 0)) * dpi, 0.01f);
        EXPECT_NEAR(scene[2].position.y, (h - 24 - assets - 4) * dpi, 0.01f);
    }
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    // Rethrows any failure captured at the window-procedure boundary.
    EXPECT_NO_THROW(engine.Tick());
}
}  // namespace hitagi::tests

TEST(EditorResizeTest, VulkanModalFrames) { hitagi::tests::CheckEditorModalResize("Vulkan"); }
TEST(EditorResizeTest, DX12ModalFrames) { hitagi::tests::CheckEditorModalResize("DX12"); }
#endif
