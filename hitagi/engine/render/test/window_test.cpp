#include "test_macros.hpp"
import interop.gtest;
#ifdef _WIN32
import interop.win32;
#endif
import std;
import core;
import app;
import gfx;
import asset;
import render;

namespace renderer_window_test {
using namespace hitagi;

template <typename DrawItem, typename Renderer>
void ExerciseWindow(Application& app, gfx::CommandQueues& queues, std::unique_ptr<Renderer> renderer) {
    Renderer&           selected = *renderer;
    core::RuntimeModule root("WindowRendererTest");
    root.AddSubModule(std::move(renderer));
    std::vector<DrawItem>      items{{.mesh = std::make_shared<asset::Mesh>()}};
    std::weak_ptr<asset::Mesh> mesh = items.front().mesh;
    selected.SetFrame({.draw_items = items});
    items.clear();
    items.shrink_to_fit();
    const auto tick = [&] {
        app.Tick();
        queues.NewFrame();
        root.Tick();
    };
    tick();
    tick();  // A window frame persists without Submit or a child producer.
    EXPECT_EQ(selected.GetRenderedFrameCount(), 2u);
    EXPECT_FALSE(mesh.expired());
    app.ResizeWindow(480, 320);
    tick();
    EXPECT_EQ(selected.GetRenderedFrameCount(), 3u);
    EXPECT_EQ(app.GetWindowWidth(), 480u);
    EXPECT_EQ(app.GetWindowHeight(), 320u);
#ifdef _WIN32
    // Exercise the app's minimized-state contract without displaying the window.
    const auto window = static_cast<HWND>(app.GetWindow().ptr);
    SendMessageW(window, WM_SIZE, SIZE_MINIMIZED, 0);
    root.Tick();
    EXPECT_EQ(selected.GetRenderedFrameCount(), 3u);
    SendMessageW(window, WM_SIZE, SIZE_RESTORED, 0);
    tick();
    EXPECT_EQ(selected.GetRenderedFrameCount(), 4u);
#endif
    selected.SetFrame({});
    EXPECT_TRUE(mesh.expired());
    tick();

#ifdef _WIN32
    // Moving the native window keeps presenting on the existing swapchain.
    // Each timer message is one deterministic frame; do not poll native events
    // from the modal callback or depend on wall-clock timer delivery.
    const auto frames_before_move = selected.GetRenderedFrameCount();
    app.SetModalFrameCallback([&] {
        EXPECT_FALSE(app.WindowSizeChanged());
        queues.NewFrame();
        root.Tick();
    });
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    for (std::uint64_t frame = 1; frame <= 4; ++frame) {
        SendMessageW(window, WM_TIMER, 1, 0);
        EXPECT_EQ(selected.GetRenderedFrameCount(), frames_before_move + frame);
        EXPECT_FALSE(app.WindowSizeChanged());
    }
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    EXPECT_FALSE(app.WindowSizeChanged());
    SendMessageW(window, WM_TIMER, 1, 0);
    EXPECT_EQ(selected.GetRenderedFrameCount(), frames_before_move + 4);
    app.SetModalFrameCallback({});

    // Consecutive resize frames may replace a swapchain before any of its
    // presented images can be reacquired. Then leave enough frames for the
    // presentation engine to release every retired swapchain.
    const auto frames_before_resize = selected.GetRenderedFrameCount();
    app.SetModalFrameCallback([&] {
        queues.NewFrame();
        root.Tick();
    });
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    for (unsigned frame = 0; frame < 24; ++frame) {
        app.ResizeWindow(480 + (frame % 6) * 16, 320 + (frame % 4) * 16);
        SendMessageW(window, WM_TIMER, 1, 0);
        EXPECT_FALSE(app.WindowSizeChanged());
    }
    for (unsigned frame = 0; frame < 8; ++frame) {
        SendMessageW(window, WM_TIMER, 1, 0);
    }
    EXPECT_EQ(selected.GetRenderedFrameCount(), frames_before_resize + 32);
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    app.SetModalFrameCallback({});
#endif

    const auto rendered_frames = selected.GetRenderedFrameCount();
    // A quit request alone does not invalidate the window or its swapchain.
    app.Quit();
    EXPECT_NO_THROW(root.Tick());
    EXPECT_EQ(selected.GetRenderedFrameCount(), rendered_frames + 1);
#ifdef _WIN32
    // Destruction changes window state without changing the cached dimensions.
    // Resize must still invalidate the native swapchain before the next present.
    const auto width  = app.GetWindowWidth();
    const auto height = app.GetWindowHeight();
    ASSERT_TRUE(PostMessageW(window, WM_CLOSE, 0, 0));
    app.Tick();
    ASSERT_TRUE(app.WindowSizeChanged());
    EXPECT_EQ(app.GetWindowWidth(), width);
    EXPECT_EQ(app.GetWindowHeight(), height);
    EXPECT_NO_THROW(root.Tick());
#endif
}

class WindowRendererTest : public testing::TestWithParam<gfx::Device::Type> {};
#ifdef _WIN32
INSTANTIATE_TEST_SUITE_P(Backends, WindowRendererTest, testing::Values(gfx::Device::Type::DX12, gfx::Device::Type::Vulkan));
#else
INSTANTIATE_TEST_SUITE_P(Backends, WindowRendererTest, testing::Values(gfx::Device::Type::Vulkan));
#endif

TEST_P(WindowRendererTest, PbrOwnsPresentationAndRetainsFrameAcrossResize) {
    auto                app    = Application::CreateApp({.width = 320, .height = 240, .headless = true});
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("WindowPbrTest");
    ExerciseWindow<render::pbr::RenderDrawItem>(*app, queues, std::make_unique<render::pbr::PbrRenderer>(asset::ResourceLoadContext{*device, queues, *bindings, compiler}, *app));
}

TEST_P(WindowRendererTest, DeferredOwnsPresentationAndRetainsFrameAcrossResize) {
    auto                app    = Application::CreateApp({.width = 320, .height = 240, .headless = true});
    auto                device = gfx::create_device(GetParam());
    gfx::CommandQueues  queues(*device);
    auto                bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler compiler("WindowDeferredTest");
    core::FileIOManager file_io;
    ExerciseWindow<render::deferred::RenderDrawItem>(*app, queues, std::make_unique<render::deferred::DeferredRenderer>(*device, queues, *bindings, compiler, file_io, "assets/shaders", *app));
}
}  // namespace renderer_window_test
