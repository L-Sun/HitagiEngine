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
    auto      result = job_system.Submit([value = std::make_unique<int>(42)] { return *value; });
    EXPECT_EQ(result.get(), 42);
    auto failed = job_system.Submit([] { throw std::runtime_error("job failure"); });
    EXPECT_THROW(failed.get(), std::runtime_error);
}

TEST(JobSystemTest, SubmitterKeepsCapturedStateAlive) {
    JobSystem job_system(2);
    auto      value     = std::make_shared<std::atomic<int>>(0);
    auto      submitter = job_system.MakeSubmitter();
    submitter([value] { value->store(42); });
    job_system.WaitForAll();
    EXPECT_EQ(value->load(), 42);
    EXPECT_FALSE(JobSubmitter{});
}

TEST(JobSystemTest, CallerExecutionPreservesThreadAndFutureContract) {
    JobSystem jobs(JobSystem::Execution::CallingThread);
    EXPECT_EQ(jobs.GetWorkerCount(), 0u);
    const auto caller = std::this_thread::get_id();
    auto       result = jobs.Submit([value = std::make_unique<int>(42), caller] {
        EXPECT_EQ(std::this_thread::get_id(), caller);
        return *value;
    });
    EXPECT_EQ(result.get(), 42);
    auto failed = jobs.Submit([] { throw std::runtime_error("caller job failure"); });
    EXPECT_THROW(failed.get(), std::runtime_error);
    jobs.WaitForAll();
}

TEST(JobSystemTest, JobGraphRejectsCyclesAndInvalidEdgesWithoutExecuting) {
    int        calls = 0;
    const auto tasks = [&] { return std::pmr::vector<JobGraph::Task>{{"a", [&] { ++calls; }}, {"b", [&] { ++calls; }}}; };
    EXPECT_THROW((JobGraph(tasks(), {{1}, {0}})), std::invalid_argument);
    EXPECT_THROW((JobGraph(tasks(), {{2}, {}})), std::invalid_argument);
    EXPECT_THROW((JobGraph(tasks(), {{}})), std::invalid_argument);
    EXPECT_EQ(calls, 0);
}
