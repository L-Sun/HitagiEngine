export module core:job_system;
import interop.taskflow;
import interop.spdlog;

import std;

import utils;

export namespace hitagi::core {

// Vocabulary type for passing execution capability across module boundaries:
// a type-erased "executor" that schedules a unit of work (fire-and-forget).
// Consumers treat an empty submitter as "no async capability available" and
// fall back to synchronous execution.
using JobSubmitter = std::function<void(std::function<void()>)>;

// A reusable, dependency-validated graph with both caller and pool execution.
class JobGraph {
public:
    struct Task {
        std::pmr::string      name;
        std::function<void()> run;
    };
    JobGraph(std::pmr::vector<Task> tasks, const std::pmr::vector<std::pmr::vector<std::size_t>>& successors);
    JobGraph(const JobGraph&)            = delete;
    JobGraph& operator=(const JobGraph&) = delete;

private:
    friend class JobSystem;
    std::pmr::vector<Task>        m_Tasks;
    std::pmr::vector<std::size_t> m_Order;
    interop::tf::Taskflow         m_Taskflow;
};

class JobSystem final {
public:
    enum class Execution { CallingThread,
                           ThreadPool };
    explicit JobSystem(std::uint32_t num_workers = 0);
    explicit JobSystem(Execution execution, std::uint32_t num_workers = 0);
    ~JobSystem() = default;

    [[nodiscard]] auto GetWorkerCount() const noexcept -> std::uint32_t { return m_NumWorkers; }
    [[nodiscard]] auto GetCurrentWorkerId() const noexcept -> int;

    template <typename Func, typename... Args>
    auto Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

    template <typename Func, typename... Args>
    auto RunTask(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

    // Type-erased executor view of this job system, for fire-and-forget submission.
    auto MakeSubmitter() -> JobSubmitter {
        return [this](std::function<void()> job) { Submit(std::move(job)); };
    }

    void Run(JobGraph& graph);
    void WaitForAll();

    JobSystem(const JobSystem&)            = delete;
    JobSystem& operator=(const JobSystem&) = delete;

private:
    std::shared_ptr<spdlog::logger>        m_Logger;
    std::uint32_t                          m_NumWorkers = 0;
    std::unique_ptr<interop::tf::Executor> m_Executor;
};

template <typename Func, typename... Args>
auto JobSystem::Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
    using return_type = std::invoke_result_t<Func, Args...>;

    if (!m_Executor) {
        std::packaged_task<return_type()> job([func = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable -> return_type {
            return std::invoke(std::move(func), std::move(args)...);
        });
        auto                              result = job.get_future();
        job();
        return result;
    }
    return m_Executor->async(
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

JobGraph::JobGraph(std::pmr::vector<Task> tasks, const std::pmr::vector<std::pmr::vector<std::size_t>>& successors)
    : m_Tasks(std::move(tasks)) {
    if (successors.size() != m_Tasks.size()) throw std::invalid_argument("Job graph size mismatch");
    std::pmr::vector<std::size_t> incoming(m_Tasks.size(), 0), ready;
    for (const auto& next : successors)
        for (const auto index : next) {
            if (index >= m_Tasks.size()) throw std::invalid_argument("Invalid job graph edge");
            ++incoming[index];
        }
    for (std::size_t i = 0; i < incoming.size(); ++i)
        if (!incoming[i]) ready.push_back(i);
    while (!ready.empty()) {
        const auto current = ready.back();
        ready.pop_back();
        m_Order.push_back(current);
        for (const auto next : successors[current])
            if (--incoming[next] == 0) ready.push_back(next);
    }
    if (m_Order.size() != m_Tasks.size()) throw std::invalid_argument("Cycle in job graph");
    std::pmr::vector<interop::tf::Task> nodes;
    for (std::size_t i = 0; i < m_Tasks.size(); ++i)
        nodes.push_back(m_Taskflow.emplace([this, i] { m_Tasks[i].run(); }).name(m_Tasks[i].name.c_str()));
    for (std::size_t i = 0; i < successors.size(); ++i)
        for (const auto next : successors[i]) nodes[i].precede(nodes[next]);
}

JobSystem::JobSystem(std::uint32_t num_workers) : JobSystem(Execution::ThreadPool, num_workers) {}
JobSystem::JobSystem(Execution execution, std::uint32_t num_workers)
    : m_Logger(utils::try_create_logger("JobSystem")),
      m_NumWorkers(execution == Execution::CallingThread ? 0 : ResolveWorkerCount(num_workers)),
      m_Executor(execution == Execution::CallingThread ? nullptr : std::make_unique<interop::tf::Executor>(m_NumWorkers)) {
    m_Logger->trace("create job system({})", m_NumWorkers);
}

void JobSystem::Run(JobGraph& graph) {
    if (m_Executor) {
        // Cooperatively execute nested query work, including on a one-worker pool.
        if (GetCurrentWorkerId() >= 0)
            m_Executor->corun(graph.m_Taskflow);
        else
            m_Executor->run(graph.m_Taskflow).get();
    } else
        for (const auto index : graph.m_Order) graph.m_Tasks[index].run();
}

auto JobSystem::GetCurrentWorkerId() const noexcept -> int {
    return m_Executor ? m_Executor->this_worker_id() : -1;
}

void JobSystem::WaitForAll() {
    if (m_Executor) m_Executor->wait_for_all();
}

}  // namespace hitagi::core
