#include "test_macros.hpp"
import std;
import core;
import app;
import ecs;

import interop.gtest;

using namespace hitagi;

TEST(AppTest, HeadlessPluginsAndSubAppsHaveIndependentWorlds) {
    core::Runtime runtime({.worker_count = 2, .log_level = "off"});
    App           app(runtime);
    app.AddPlugin(TimePlugin{});
    auto& sub = app.AddSubApp("Preview", [](ecs::World& main, ecs::World& preview) {
        preview.GetResource<int>() = static_cast<int>(main.GetResource<Time>().frame);
    });
    sub.GetWorld().EmplaceResource<int>(0);
    sub.AddPlugin(TimePlugin{});
    EXPECT_EQ(app.Run(3), 0);
    EXPECT_EQ(app.GetWorld().GetResource<Time>().frame, 3u);
    EXPECT_EQ(sub.GetWorld().GetResource<Time>().frame, 3u);
    EXPECT_EQ(sub.GetWorld().GetResource<int>(), 3);
    EXPECT_FALSE(app.GetWorld().HasResource<int>());
}

TEST(AppTest, FrameCleanupRunsOnFailureAndPreventsReentry) {
    core::Runtime runtime({.worker_count = 1, .log_level = "off"});
    App           app(runtime);
    int           cleanup = 0;
    int           calls   = 0;
    app.SetFrameCleanup([&] { ++cleanup; app.TickFrame(); });
    app.GetWorld().AddTask("fail", [&] { ++calls; throw std::runtime_error("frame"); });
    EXPECT_THROW(app.Tick(), std::runtime_error);
    EXPECT_THROW(app.Tick(), std::runtime_error);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(cleanup, 2);
}

TEST(AppTest, FailedPluginCannotRunPartialApp) {
    struct Failure {
        void Build(App& app) const {
            app.GetWorld().EmplaceResource<int>(1);
            throw std::runtime_error("failure");
        }
    };
    core::Runtime runtime({.worker_count = 1, .log_level = "off"});
    App           app(runtime);
    EXPECT_THROW(app.AddPlugin(Failure{}), std::runtime_error);
    EXPECT_THROW(app.Tick(), std::logic_error);
}

TEST(AppTest, AppWaitsForJobsWhileItsResourcesRemainAlive) {
    struct State {
        std::atomic<int> value = 0;
    };
    core::Runtime     runtime({.worker_count = 1, .log_level = "off"});
    std::future<void> result;
    bool              observed = false;
    {
        App   app(runtime);
        auto& state = app.GetWorld().EmplaceResource<State>();
        result      = runtime.Jobs().Submit([&] { state.value = 42; });
        app.OnShutdown([&] { observed = state.value == 42; });
    }
    result.get();
    EXPECT_TRUE(observed);
}
