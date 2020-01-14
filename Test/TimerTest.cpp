#include <gtest/gtest.h>
#include "Timer.hpp"
#include <thread>

using namespace My;
using namespace std;
using namespace std::chrono_literals;

My::Clock c;

TEST(TimerTest, DurationTest) {
    c.Initialize();
    c.Start();
    std::this_thread::sleep_for(1s);
    c.Tick();
    EXPECT_NEAR(1.0, c.deltaTime(), 0.1);
}

TEST(TimerTest, PauseTest) {
    c.Initialize();
    c.Start();
    std::this_thread::sleep_for(1s);
    c.Tick();
    c.Pause();
    std::this_thread::sleep_for(1s);
    c.Start();
    EXPECT_NEAR(1.0, c.deltaTime(), 0.1);
}

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
    return 0;
}
