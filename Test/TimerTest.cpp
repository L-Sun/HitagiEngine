#include <gtest/gtest.h>
#include "Timer.hpp"
#include <thread>

using namespace std::chrono_literals;

Hitagi::Core::Clock g_C;

TEST(TimerTest, DurationTest) {
    g_C.Start();
    std::this_thread::sleep_for(1s);
    EXPECT_NEAR(1.0, g_C.DeltaTime().count(), 0.1);
}

TEST(TimerTest, PauseTest) {
    g_C.Start();
    std::this_thread::sleep_for(1s);
    g_C.Pause();
    std::this_thread::sleep_for(1s);
    EXPECT_NEAR(1.0, g_C.DeltaTime().count(), 0.1);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
