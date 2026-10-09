export module core:job_system;
import interop.taskflow;
import interop.spdlog;

import std;

import :runtime_module;

export namespace hitagi::core {

// Vocabulary type for passing execution capability across module boundaries:
// a type-erased "executor" that schedules a unit of work (fire-and-forget).
// Consumers treat an empty submitter as "no async capability available" and
// fall back to synchronous execution.
using JobSubmitter = std::function<void(std::move_only_function<void()>)>;

class JobSystem final : public RuntimeModule {
public:
    explicit JobSystem(std::uint32_t num_workers = 0);
    ~JobSystem() final = default;

    [[nodiscard]] auto GetWorkerCount() const noexcept -> std::uint32_t { return m_NumWorkers; }
    [[nodiscard]] auto GetCurrentWorkerId() const noexcept -> int;

    template <typename Func, typename... Args>
    auto Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

    template <typename Func, typename... Args>
    auto RunTask(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

    // Type-erased executor view of this job system, for fire-and-forget submission.
    auto MakeSubmitter() -> JobSubmitter {
        return [this](std::move_only_function<void()> job) { Submit(std::move(job)); };
    }

    void RunTaskflow(interop::tf::Taskflow& taskflow);
    void WaitForAll();

    JobSystem(const JobSystem&)            = delete;
    JobSystem& operator=(const JobSystem&) = delete;

private:
    std::uint32_t         m_NumWorkers = 0;
    interop::tf::Executor m_Executor;
};

template <typename Func, typename... Args>
auto JobSystem::Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
    using return_type = std::invoke_result_t<Func, Args...>;

    return m_Executor.async(
        [func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable -> return_type {
            return std::invoke(std::move(func), std::move(args)...);
        });
}

template <typename Func, typename... Args>
auto JobSystem::RunTask(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
    return Submit(std::forward<Func>(func), std::forward<Args>(args)...);
}

}  // namespace hitagi::core

namespace hitagi::core {

static auto ResolveWorkerCount(std::uint32_t requested_workers) noexcept -> std::uint32_t {
    if (requested_workers != 0) {
        return requested_workers;
    }
    return std::max(1u, std::thread::hardware_concurrency());
}

JobSystem::JobSystem(std::uint32_t num_workers)
    : RuntimeModule("JobSystem"),
      m_NumWorkers(ResolveWorkerCount(num_workers)),
      m_Executor(m_NumWorkers) {
    m_Logger->trace("create job system({})", m_NumWorkers);
}

auto JobSystem::GetCurrentWorkerId() const noexcept -> int {
    return m_Executor.this_worker_id();
}

void JobSystem::RunTaskflow(interop::tf::Taskflow& taskflow) {
    m_Executor.run(taskflow).wait();
}

void JobSystem::WaitForAll() {
    m_Executor.wait_for_all();
}

}  // namespace hitagi::core
