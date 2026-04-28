module;

#include <cassert>
#include <spdlog/logger.h>
#include <taskflow/core/executor.hpp>
#include <taskflow/taskflow.hpp>
#include <tracy/Tracy.hpp>

export module core;
import std;
import utils;

export namespace hitagi::core {

class RuntimeModule {
public:
    RuntimeModule(std::string_view name);
    virtual ~RuntimeModule();
    virtual void Tick();

    inline auto GetName() const noexcept -> std::string_view { return m_Name; };

    auto         GetSubModule(std::string_view name) -> RuntimeModule*;
    auto         GetSubModules() const noexcept -> std::pmr::vector<RuntimeModule*>;
    virtual auto AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after = nullptr) -> RuntimeModule*;
    virtual void UnloadSubModule(std::string_view name);

    static auto GetModule(std::string_view name) -> RuntimeModule*;

protected:
    std::pmr::string                               m_Name;
    std::shared_ptr<spdlog::logger>                m_Logger;
    std::pmr::list<std::unique_ptr<RuntimeModule>> m_SubModules;

    static std::unordered_map<std::string, RuntimeModule*> sm_AllModules;
};

class Buffer {
public:
    Buffer() = default;
    Buffer(std::size_t size, const std::byte* data = nullptr, std::size_t alignment = 4);
    Buffer(std::span<const std::byte> data, std::size_t alignment = 4);

    Buffer(const Buffer& buffer);
    Buffer(Buffer&& buffer) noexcept;

    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs) noexcept;

    ~Buffer();

    void Resize(std::size_t size, std::size_t alignment = 4);

    inline std::byte*       GetData() noexcept { return m_Data; }
    inline const std::byte* GetData() const noexcept { return m_Data; }
    inline auto             GetDataSize() const noexcept { return m_Size; }
    inline bool             Empty() const noexcept { return m_Data == nullptr || m_Size == 0; }

    template <typename T>
    std::span<const T> Span() const {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<const T>(reinterpret_cast<const T*>(m_Data), m_Size / sizeof(T));
    }

    template <typename T>
    std::span<T> Span() {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<T>(reinterpret_cast<T*>(m_Data), m_Size / sizeof(T));
    }

    auto Str() const noexcept {
        return std::string_view{reinterpret_cast<const char*>(m_Data), m_Size};
    }

private:
    std::pmr::polymorphic_allocator<> m_Allocator;
    std::byte*                        m_Data      = nullptr;
    std::size_t                       m_Size      = 0;
    std::size_t                       m_Alignment = alignof(std::uint32_t);
};

class MemoryManager final : public RuntimeModule {
public:
    MemoryManager();
    ~MemoryManager() final;

    template <typename T = std::byte>
    std::pmr::polymorphic_allocator<T> GetAllocator() const noexcept;

private:
    std::unique_ptr<std::pmr::memory_resource> m_Pools;
};

template <typename T>
std::pmr::polymorphic_allocator<T> MemoryManager::GetAllocator() const noexcept {
    return std::pmr::polymorphic_allocator<T>(m_Pools.get());
}

class Clock {
public:
    Clock();

    std::chrono::duration<double>                  TotalTime() const;
    std::chrono::duration<double>                  DeltaTime() const;
    std::chrono::high_resolution_clock::time_point GetBaseTime() const;
    std::chrono::high_resolution_clock::time_point TickTime() const;

    void        Reset();
    void        Start();
    void        Tick();
    void        Pause();
    inline bool IsPaused() const noexcept { return m_Paused; }

private:
    std::chrono::duration<double> m_PausedTime = std::chrono::duration<double>::zero();

    std::chrono::high_resolution_clock::time_point m_BaseTime;
    std::chrono::high_resolution_clock::time_point m_StopTime;
    std::chrono::high_resolution_clock::time_point m_TickTime;

    std::chrono::duration<double> m_DeltaTime = std::chrono::duration<double>::zero();

    bool m_Paused = true;
};

class JobSystem final : public RuntimeModule {
public:
    explicit JobSystem(std::uint32_t num_workers = 0);
    ~JobSystem() final;

    inline static auto Get() {
        return static_cast<JobSystem*>(RuntimeModule::GetModule("JobSystem"));
    }

    [[nodiscard]] auto GetWorkerCount() const noexcept -> std::uint32_t { return m_NumWorkers; }
    [[nodiscard]] auto GetCurrentWorkerId() const noexcept -> int;

    template <typename Func, typename... Args>
    auto Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

    template <typename Func, typename... Args>
    auto RunTask(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>;

    void RunTaskflow(tf::Taskflow& taskflow);
    void WaitForAll();

    JobSystem(const JobSystem&)            = delete;
    JobSystem& operator=(const JobSystem&) = delete;

private:
    std::uint32_t m_NumWorkers = 0;
    tf::Executor  m_Executor;
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

class FileIOManager : public RuntimeModule {
public:
    FileIOManager() : RuntimeModule("FileIOManager") {}

    inline static auto Get() {
        return static_cast<FileIOManager*>(RuntimeModule::GetModule("FileIOManager"));
    }

    auto SyncOpenAndReadBinary(const std::filesystem::path& file_path) -> const Buffer&;
    void SaveString(std::string_view str, const std::filesystem::path& path);
    void SaveBuffer(const Buffer& buffer, const std::filesystem::path& path);
    void SaveBuffer(std::span<const std::byte> buffer, const std::filesystem::path& path);

private:
    bool          IsFileChanged(const std::filesystem::path& file_path) const;
    const Buffer& CacheFile(const std::filesystem::path& file_path, Buffer buffer);

    using PathHash = std::size_t;

    TracyLockableN(std::mutex, m_CacheMutex, "FileIO Cache Mutex");
    std::pmr::unordered_map<PathHash, std::filesystem::file_time_type> m_FileStateCache;
    std::pmr::unordered_map<PathHash, Buffer>                          m_FileCache;
    Buffer                                                             m_EmptyBuffer;
};

}  // namespace hitagi::core
