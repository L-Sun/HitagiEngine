#include <hitagi/utils/test.hpp>
#include <hitagi/core/timer.hpp>

#include <thread>

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

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::off);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
