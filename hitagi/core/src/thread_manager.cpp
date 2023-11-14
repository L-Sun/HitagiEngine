#include <hitagi/core/thread_manager.hpp>

#include <spdlog/logger.h>

namespace hitagi::core {

ThreadManager::ThreadManager(std::uint8_t num_threads) : RuntimeModule("ThreadManager"), m_Stop(false) {
    m_Logger->trace("create thread pool({})", num_threads);

    for (std::uint8_t i = 0; i < num_threads; i++) {
        m_ThreadPools.emplace_back([this] {
            while (true) {
                std::packaged_task<void()> task;
                {
                    std::unique_lock lock(m_QueueMutex);
                    m_ConditionForTask.wait(lock, [this] { return m_Stop || !m_Tasks.empty(); });
                    if (m_Stop && m_Tasks.empty())
                        return;
                    task = std::move(m_Tasks.front());
                    m_Tasks.pop();
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