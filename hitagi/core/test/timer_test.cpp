#include "test_macros.hpp"
#include <spdlog/spdlog.h>
#include <thread>

import core;
import test_utils;

using namespace std::chrono_literals;
using namespace hitagi::core;

TEST(TimerTest, DurationTest) {
    Clock clock;
    clock.Start();
    std::this_thread::sleep_for(0.01s);
    EXPECT_NEAR(0.01, clock.DeltaTime().count(), 0.1);
}

TEST(TimerTest, PauseTest) {
    Clock clock;
    clock.Start();
    std::this_thread::sleep_for(0.01s);
    clock.Pause();
    std::this_thread::sleep_for(0.01s);
    EXPECT_NEAR(0.01, clock.DeltaTime().count(), 0.1);
}
