#include "test_macros.hpp"
import interop.gtest;

import std;
import platform;
import core;
import gui;

using namespace hitagi;

class PocketGuiManagerTest : public ::testing::Test {
protected:
    core::IOService                         file_io;
    std::unique_ptr<platform::PlatformHost> app = platform::PlatformHost::Create({
        .title    = "PocketJS test",
        .width    = 800,
        .height   = 800,
        .headless = true,
    });
    gui::GuiContext                         manager{*app, file_io};
};

TEST_F(PocketGuiManagerTest, EmptyUntilExplicitLoad) {
    manager.Compose();
    EXPECT_TRUE(manager.GetDrawData().Empty());
}

TEST_F(PocketGuiManagerTest, PostEventRequiresLoadedApp) {
    EXPECT_THROW(manager.PostEvent("{}"), std::logic_error);
}
