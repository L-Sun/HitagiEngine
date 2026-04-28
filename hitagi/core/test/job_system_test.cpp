#include "test_macros.hpp"

import core;

using namespace hitagi::core;

TEST(JobSystemTest, SubmitRunsTask) {
    auto* job_system = JobSystem::Get();

    ASSERT_NE(job_system, nullptr);

    auto result = job_system->Submit([] { return 42; });
    EXPECT_EQ(result.get(), 42);
}

TEST(JobSystemTest, RunTaskRunsTask) {
    auto* job_system = JobSystem::Get();

    ASSERT_NE(job_system, nullptr);

    auto result = job_system->RunTask([] { return 7; });
    EXPECT_EQ(result.get(), 7);
}
