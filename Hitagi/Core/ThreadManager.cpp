#include "ThreadManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi {
std::unique_ptr<Core::ThreadManager> g_ThreadManager = std::make_unique<Core::ThreadManager>();
}

namespace Hitagi::Core {

int ThreadManager::Initialize() {
    m_Logger        = spdlog::stdout_color_mt("ThreadManager");
    auto num_threads = std::thread::hardware_concurrency();
    m_MaxTasks      = 1024;
    m_Stop          = false;

    m_Logger->info("Initialize... Num of Thread: {}, Max Num of Task: {}", num_threads, m_MaxTasks);

    for (decltype(num_threads) i = 0; i < num_threads; i++) {
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
                    m_ConditionForTask.notify_one();
                }
                task();
            }
        });
    }

    return 0;
}
void ThreadManager::Finalize() {
    {
        std::unique_lock lock(m_QueueMutex);
        m_Stop = true;
    }
    m_ConditionForTask.notify_all();
    for (std::thread& thread : m_ThreadPools) {
        thread.join();
    }
    m_Logger->info("Finalize.");
}

void ThreadManager::Tick() {}

}  // namespace Hitagi::Core