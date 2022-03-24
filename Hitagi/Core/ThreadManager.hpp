#pragma once
#include "IRuntimeModule.hpp"

#include <memory>
#include <mutex>
#include <future>
#include <queue>
#include <vector>

namespace hitagi::core {

class ThreadManager : public IRuntimeModule {
public:
    ThreadManager() = default;

    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    template <typename Func, typename... Args>
    decltype(auto) RunTask(Func&& func, Args&&... args);

    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

private:
    size_t m_MaxTasks = 1;

    std::vector<std::thread>               m_ThreadPools;
    std::queue<std::packaged_task<void()>> m_Tasks;

    std::mutex              m_QueueMutex;
    std::condition_variable m_ConditionForTask;
    std::condition_variable m_ConditionForQueueSize;
    bool                    m_Stop = true;
};

template <typename Func, typename... Args>
decltype(auto) ThreadManager::RunTask(Func&& func, Args&&... args) {
    if (m_Stop)
        throw std::runtime_error("Run a task on stopped thread pool.");

    using return_type = std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock lock(m_QueueMutex);
        m_ConditionForQueueSize.wait(lock, [this] { return m_Tasks.size() < m_MaxTasks; });
        if (m_Stop)
            throw std::runtime_error("Run a task on stopped thread pool.");

        m_Tasks.emplace([task] { (*task)(); });
    }
    m_ConditionForTask.notify_one();
    return res;
}

}  // namespace hitagi::core

namespace hitagi {
extern std::unique_ptr<core::ThreadManager> g_ThreadManager;
}
