#pragma once
#include <hitagi/core/runtime_module.hpp>

#include <memory>
#include <mutex>
#include <future>
#include <queue>
#include <vector>
#include <functional>

namespace hitagi::core {

class ThreadManager final : public RuntimeModule {
public:
    ThreadManager(std::uint8_t num_threads = 8);
    ~ThreadManager() final;

    template <typename Func, typename... Args>
    decltype(auto) RunTask(Func&& func, Args&&... args);

    ThreadManager(const ThreadManager&)            = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

private:
    std::pmr::vector<std::thread>                                                       m_ThreadPools;
    std::queue<std::packaged_task<void()>, std::pmr::deque<std::packaged_task<void()>>> m_Tasks;

    std::mutex              m_QueueMutex;
    std::condition_variable m_ConditionForTask;
    std::condition_variable m_ConditionForQueueSize;
    bool                    m_Stop;
};

template <typename Func, typename... Args>
decltype(auto) ThreadManager::RunTask(Func&& func, Args&&... args) {
    using return_type = std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock lock(m_QueueMutex);
        m_Tasks.emplace([task] { (*task)(); });
    }

    m_ConditionForTask.notify_one();
    return res;
}

}  // namespace hitagi::core
