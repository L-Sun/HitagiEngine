#include "test_macros.hpp"
import std;
import interop.gtest;
import interop.taskflow;

namespace interop = hitagi::interop;
static_assert(std::same_as<interop::tf::Taskflow, ::tf::Taskflow>);
static_assert(std::same_as<interop::tf::Executor, ::tf::Executor>);

TEST(TaskflowInteropTest, OrdersTasksAndRebuildsGraph) {
    interop::tf::Executor executor(2);
    interop::tf::Taskflow graph;
    std::vector<int>      order;
    auto                  first  = graph.emplace([&] { order.push_back(1); });
    auto                  second = graph.emplace([&] { order.push_back(2); });
    first.name("first").precede(second);
    executor.run(graph).get();
    EXPECT_EQ(order, (std::vector<int>{1, 2}));

    graph.clear();
    graph.emplace([&] { order.push_back(3); });
    executor.run(graph).get();
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(TaskflowInteropTest, AsyncPreservesResultsExceptionsAndMoveOnlyWork) {
    interop::tf::Executor executor(2);
    EXPECT_EQ(executor.this_worker_id(), -1);
    auto result = executor.async([value = std::make_unique<int>(42)] { return *value; });
    EXPECT_EQ(result.get(), 42);
    auto worker = executor.async([&] { return executor.this_worker_id(); });
    EXPECT_GE(worker.get(), 0);

    auto failure = executor.async([]() -> int { throw std::runtime_error("task failed"); });
    EXPECT_THROW(failure.get(), std::runtime_error);

    std::atomic<int> completed = 0;
    for (int i = 0; i < 8; ++i) {
        executor.async([&] { ++completed; });
    }
    executor.wait_for_all();
    EXPECT_EQ(completed.load(), 8);
}

TEST(TaskflowInteropTest, NativeSubflowsCompositionAndRepeatedRuns) {
    interop::tf::Executor executor(2);
    interop::tf::Taskflow graph;
    interop::tf::Taskflow child;
    std::atomic<int>      completed = 0;
    child.emplace([&] { ++completed; });
    auto module  = graph.composed_of(child);
    auto subflow = graph.emplace([&](interop::tf::Subflow& flow) {
        auto first  = flow.emplace([&] { ++completed; });
        auto second = flow.emplace([&] { ++completed; });
        first.precede(second);
    });
    module.precede(subflow);
    interop::tf::Future<void> done = executor.run_n(graph, 3);
    done.get();
    EXPECT_EQ(completed.load(), 9);
    EXPECT_EQ(graph.num_tasks(), 2);
}

TEST(TaskflowInteropTest, NativeRuntimeAndProfiler) {
    interop::tf::Executor executor(2);
    interop::tf::Taskflow graph;
    std::atomic<int>      completed = 0;
    auto                  observer  = executor.make_observer<interop::tf::TFProfObserver>();
    graph.emplace([&](interop::tf::Runtime& runtime) {
             runtime.silent_async([&] { ++completed; });
         })
        .name("runtime");
    executor.run(graph).get();
    EXPECT_EQ(completed.load(), 1);
    EXPECT_GT(observer->num_tasks(), 0);
    EXPECT_FALSE(observer->summary().empty());
    executor.remove_observer(observer);
}
