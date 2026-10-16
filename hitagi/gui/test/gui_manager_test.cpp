#include "test_macros.hpp"
import interop.gtest;

import std;
import app;
import core;
import gui;

using namespace hitagi;

class PocketGuiManagerTest : public ::testing::Test {
protected:
    core::FileIOManager          file_io;
    std::unique_ptr<Application> app = Application::CreateApp({
        .title    = "PocketJS test",
        .width    = 800,
        .height   = 800,
        .headless = true,
    });
    gui::GuiManager              manager{*app, file_io};
};

TEST_F(PocketGuiManagerTest, EmptyUntilExplicitLoad) {
    manager.Tick();
    EXPECT_TRUE(manager.GetDrawData().Empty());
}

TEST_F(PocketGuiManagerTest, PostEventRequiresLoadedApp) {
    EXPECT_THROW(manager.PostEvent("{}"), std::logic_error);
}
