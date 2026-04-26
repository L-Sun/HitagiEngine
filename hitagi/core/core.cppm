module;

#include <cassert>
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>

export module core;
import std;
import utils;

export namespace hitagi {

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

namespace core {

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

class MemoryPool;

class MemoryManager final : public RuntimeModule {
public:
    MemoryManager();
    ~MemoryManager() final;

    template <typename T = std::byte>
    std::pmr::polymorphic_allocator<T> GetAllocator() const noexcept;

private:
    std::unique_ptr<MemoryPool> m_Pools;
};

template <typename T>
std::pmr::polymorphic_allocator<T> MemoryManager::GetAllocator() const noexcept {
    return std::pmr::polymorphic_allocator<T>(m_Pools.get());
}

class MemoryPool : public std::pmr::memory_resource {
public:
    MemoryPool(std::shared_ptr<spdlog::logger> logger);
    MemoryPool(const MemoryPool&)            = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    ~MemoryPool();

private:
    [[nodiscard]] void* do_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) final;
    void                do_deallocate(void* p, std::size_t bytes, std::size_t alignment) final;
    bool                do_is_equal(const std::pmr::memory_resource& other) const noexcept final {
        return this == &other;
    }

    struct Block {
        Block* next = nullptr;
    };

    class Page {
    public:
        Page(std::size_t page_size, std::size_t block_size);
        Page(const Page&)            = delete;
        Page& operator=(const Page&) = delete;
        Page(Page&&) noexcept;
        Page& operator=(Page&&) noexcept;
        ~Page();

        inline auto GetHeadBlock() noexcept { return reinterpret_cast<Block*>(data); }

    private:
        const std::size_t      size;
        const std::align_val_t alignment;
        std::byte*             data;
    };

    struct Pool {
        TracyLockableN(std::mutex, mutex, "MemoryPool Mutex");
        std::list<Page> pages{};
        Block*          free_list       = nullptr;
        std::size_t     page_size       = 8_kB;
        std::size_t     block_size      = 0;
        std::size_t     num_free_blocks = 0;

        Page&                new_page();
        [[nodiscard]] Block* allocate();
        void                 deallocate(Block* block);

#ifdef HITAGI_DEBUG
        std::unordered_set<Block*> allocated_blocks = {};
#endif
    };

    constexpr static std::array block_size = {
        // 4-byte increments
        8u, 12u, 16u, 20u, 24u, 28u, 32u, 36u, 40u, 44u, 48u, 52u, 56u, 60u, 64u, 68u, 72u, 76u, 80u, 84u, 88u, 92u, 96u,

        // 32-byte increments
        128u, 160u, 192u, 224u, 256u, 288u, 320u, 352u, 384u, 416u, 448u, 480u, 512u, 544u, 576u, 608u, 640u,

        // 64-byte increments
        704u, 768u, 832u, 896u, 960u, 1024u};

    std::array<std::size_t, block_size.back() + 1> pool_map;
    utils::optional_ref<Pool>                      GetPool(std::size_t bytes);

    std::array<Pool, block_size.size()> m_Pools;

    template <std::size_t... Ns>
    constexpr auto InitPools(std::index_sequence<Ns...>) {
        return std::array{(Pool{.block_size = block_size.at(Ns)})...};
    }

    std::shared_ptr<spdlog::logger> m_Logger;
};

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

auto CreateThreadManager(std::uint8_t num_threads = 8) -> std::unique_ptr<RuntimeModule>;

template <typename Func, typename... Args>
decltype(auto) ThreadManager::RunTask(Func&& func, Args&&... args) {
    using return_type = std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock lock(m_QueueMutex);
        m_Tasks.emplace([task] { (*task)(); });
        TracyPlot("Thread Tasks Pending", static_cast<std::int64_t>(m_Tasks.size()));
    }

    m_ConditionForTask.notify_one();
    return res;
}

inline auto CreateThreadManager(std::uint8_t num_threads) -> std::unique_ptr<RuntimeModule> {
    return std::make_unique<ThreadManager>(num_threads);
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

}  // namespace core

}  // namespace hitagi
