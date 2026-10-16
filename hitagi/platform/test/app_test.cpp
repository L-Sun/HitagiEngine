#include "test_macros.hpp"
import interop.gtest;
import interop.spdlog;
#ifdef _WIN32
import interop.win32;
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
