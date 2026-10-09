#include "test_macros.hpp"
import std;
import interop.gtest;

import core;

using namespace hitagi::core;

TEST(JobSystemTest, SubmitRunsTask) {
    JobSystem job_system;

    auto result = job_system.Submit([] { return 42; });
    EXPECT_EQ(result.get(), 42);
}

TEST(JobSystemTest, RunTaskRunsTask) {
    JobSystem job_system;

    auto result = job_system.RunTask([] { return 7; });
    EXPECT_EQ(result.get(), 7);
}
