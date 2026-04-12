module;
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>

module core;

namespace hitagi::core {

ThreadManager::ThreadManager(std::uint8_t num_threads) : RuntimeModule("ThreadManager"), m_Stop(false) {
    m_Logger->trace("create thread pool({})", num_threads);
    TracyPlotConfig("Thread Tasks Pending", tracy::PlotFormatType::Number, true, true, 0);

    for (std::uint8_t i = 0; i < num_threads; i++) {
        m_ThreadPools.emplace_back([this, i] {
            const auto thread_name = std::format("Hitagi/Worker-{}", i);
            tracy::SetThreadName(thread_name.c_str());
            while (true) {
                std::packaged_task<void()> task;
                {
                    std::unique_lock lock(m_QueueMutex);
                    m_ConditionForTask.wait(lock, [this] { return m_Stop || !m_Tasks.empty(); });
                    if (m_Stop && m_Tasks.empty())
                        return;
                    task = std::move(m_Tasks.front());
                    m_Tasks.pop();
                    TracyPlot("Thread Tasks Pending", static_cast<std::int64_t>(m_Tasks.size()));
                }
                task();
            }
        });
    }
}

ThreadManager::~ThreadManager() {
    {
        std::unique_lock lock(m_QueueMutex);
        m_Stop = true;
    }
    m_ConditionForTask.notify_all();
    for (std::thread& thread : m_ThreadPools) {
        thread.join();
    }
}

}  // namespace hitagi::core
