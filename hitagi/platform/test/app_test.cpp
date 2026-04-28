#include "test_macros.hpp"

import app;
import test_utils;

using namespace hitagi;

class AppTest : public ::testing::Test {
protected:
    AppTest()
        : app(Application::CreateApp({
              .title    = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
              .headless = true,
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
