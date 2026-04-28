module;
#include <spdlog/logger.h>
#include <taskflow/taskflow.hpp>

module core;

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

JobSystem::~JobSystem() = default;

auto JobSystem::GetCurrentWorkerId() const noexcept -> int {
    return m_Executor.this_worker_id();
}

void JobSystem::RunTaskflow(tf::Taskflow& taskflow) {
    m_Executor.run(taskflow).wait();
}

void JobSystem::WaitForAll() {
    m_Executor.wait_for_all();
}

}  // namespace hitagi::core
