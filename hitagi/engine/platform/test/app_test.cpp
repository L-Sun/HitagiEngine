#include "test_macros.hpp"
import interop.gtest;
import interop.spdlog;
#ifdef _WIN32
import interop.win32;
#else
import interop.sdl;
#endif
import std;
import app;
import test_utils;

using namespace hitagi;

class AppTest : public ::testing::Test {
protected:
    AppTest()
        : app(Application::CreateApp({
              .title     = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
              .log_level = spdlog::level::to_string_view(spdlog::get_level()).data(),
              .headless  = true,
          })) {}
    std::unique_ptr<Application> app;
};

TEST_F(AppTest, CreateApp) {
    EXPECT_NE(app, nullptr);
}

TEST_F(AppTest, ResizeWindow) {
    app->ResizeWindow(800, 600);
    auto rect = app->GetWindowRect();
    EXPECT_EQ(rect.right - rect.left, 800);
    EXPECT_EQ(rect.bottom - rect.top, 600);
}

#ifdef _WIN32
TEST_F(AppTest, BackgroundErasePreservesRenderedPixels) {
    const auto window = static_cast<HWND>(app->GetWindow().ptr);
    const auto window_dc = GetDC(window);
    const auto dc = CreateCompatibleDC(window_dc);
    const auto bitmap = CreateCompatibleBitmap(window_dc, 16, 16);
    ASSERT_NE(dc, nullptr);
    ASSERT_NE(bitmap, nullptr);
    const auto previous = SelectObject(dc, bitmap);
    constexpr unsigned color = 0x00332211;
    SetPixel(dc, 8, 8, color);

    // The native erase runs between GPU presents during move/resize. Read back
    // its supplied DC, not the render target (which cannot see a GDI overwrite).
    const auto handled = SendMessageW(window, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0);
    const auto pixel = GetPixel(dc, 8, 8);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(window, window_dc);

    EXPECT_NE(handled, 0);
    EXPECT_EQ(pixel, color);
}

TEST_F(AppTest, MoveLoopWithoutResizeDoesNotInvalidateWindowSize) {
    app->Tick();
    ASSERT_FALSE(app->WindowSizeChanged());
    const auto window = static_cast<HWND>(app->GetWindow().ptr);
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    EXPECT_FALSE(app->WindowSizeChanged());
}

TEST_F(AppTest, ModalSizingFramesAreNonReentrantAndStopOnExit) {
    const auto window = static_cast<HWND>(app->GetWindow().ptr);
    int        frames = 0;
    app->SetModalFrameCallback([&] {
        ++frames;
        EXPECT_EQ(app->GetWindowWidth(), 900u);
        const bool resized = app->WindowSizeChanged();
        SendMessageW(window, WM_TIMER, 1, 0);  // Must not re-enter or consume the resize.
        EXPECT_EQ(app->WindowSizeChanged(), resized);
    });
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    app->ResizeWindow(900, 600);
    SendMessageW(window, WM_TIMER, 1, 0);
    EXPECT_EQ(frames, 1);
    SendMessageW(window, WM_TIMER, 1, 0);
    EXPECT_EQ(frames, 2);
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    SendMessageW(window, WM_TIMER, 1, 0);
    EXPECT_EQ(frames, 2);
    app->SetModalFrameCallback({});
}

TEST_F(AppTest, ModalFrameErrorsReturnThroughTickNotWindowProc) {
    const auto window = static_cast<HWND>(app->GetWindow().ptr);
    app->SetModalFrameCallback([] { throw std::runtime_error("modal frame failure"); });
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    EXPECT_NO_THROW(SendMessageW(window, WM_TIMER, 1, 0));
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    EXPECT_THROW(app->Tick(), std::runtime_error);
    app->SetModalFrameCallback({});
}
#endif

#ifndef _WIN32
TEST_F(AppTest, LiveResizeFramesAreImmediateAndNonReentrant) {
    app->Tick();
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_EXPOSED;
    event.window.windowID = SDL_GetWindowID(static_cast<SDL_Window*>(app->GetWindow().ptr));
    event.window.data1 = 1;
    int frames = 0;
    app->SetModalFrameCallback([&] {
        ++frames;
        if (frames == 1) SDL_PushEvent(&event);
    });
    // No Tick/PollEvent: native live resize cannot return to the outer loop.
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(frames, 1);
    event.window.data1 = 0;
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(frames, 1);
    event.window.data1 = 1;
    ++event.window.windowID;
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(frames, 1);
    --event.window.windowID;
    std::thread worker([&] { SDL_PushEvent(&event); });
    worker.join();
    EXPECT_EQ(frames, 1);
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(frames, 2);
    app->SetModalFrameCallback({});
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(frames, 2);
}

TEST_F(AppTest, LiveResizeErrorsReturnThroughTickNotSDL) {
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_EXPOSED;
    event.window.windowID = SDL_GetWindowID(static_cast<SDL_Window*>(app->GetWindow().ptr));
    event.window.data1 = 1;
    app->SetModalFrameCallback([] { throw std::runtime_error("modal frame failure"); });
    EXPECT_NO_THROW(SDL_PushEvent(&event));
    EXPECT_THROW(app->Tick(), std::runtime_error);
    app->SetModalFrameCallback({});
}
#endif
