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

TEST(JobSystemTest, SubmitPreservesMoveOnlyCapturesAndExceptions) {
    JobSystem job_system(2);
    auto result = job_system.Submit([value = std::make_unique<int>(42)] { return *value; });
    EXPECT_EQ(result.get(), 42);
    auto failed = job_system.Submit([] { throw std::runtime_error("job failure"); });
    EXPECT_THROW(failed.get(), std::runtime_error);
}

TEST(JobSystemTest, SubmitterKeepsCapturedStateAlive) {
    JobSystem job_system(2);
    auto value = std::make_shared<std::atomic<int>>(0);
    auto submitter = job_system.MakeSubmitter();
    submitter([value] { value->store(42); });
    job_system.WaitForAll();
    EXPECT_EQ(value->load(), 42);
    EXPECT_FALSE(JobSubmitter{});
}
