#include <hitagi/core/timer.hpp>

#include <gtest/gtest.h>

#include <thread>

using namespace std::chrono_literals;
using namespace hitagi::core;

TEST(TimerTest, DurationTest) {
    Clock clock;
    clock.Start();
    std::this_thread::sleep_for(1s);
    EXPECT_NEAR(1.0, clock.DeltaTime().count(), 0.1);
}

TEST(TimerTest, PauseTest) {
    Clock clock;
    clock.Start();
    std::this_thread::sleep_for(1s);
    clock.Pause();
    std::this_thread::sleep_for(1s);
    EXPECT_NEAR(1.0, clock.DeltaTime().count(), 0.1);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
