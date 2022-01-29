#pragma once
#include "IRuntimeModule.hpp"

#include <list>
#include <array>
#include <memory_resource>

namespace Hitagi::Core {

constexpr size_t operator""_kB(unsigned long long val) { return val << 10; }

class MemoryPool : public std::pmr::memory_resource {
public:
    MemoryPool(std::shared_ptr<spdlog::logger> logger);
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

private:
    [[nodiscard]] void* do_allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) final;
    void                do_deallocate(void* p, size_t bytes, size_t alignment) final;
    bool                do_is_equal(const std::pmr::memory_resource& other) const noexcept final {
        return this == &other;
    }

    struct Block {
        Block* next = nullptr;
    };

    using Page = std::unique_ptr<std::byte[]>;

    struct Pool {
        std::mutex m_Mutex;

        std::list<Page> pages;
        Block*          free_list = nullptr;
        size_t          page_size;
        size_t          block_size;
        size_t          num_free_blocks = 0;

        Page&                new_page();
        [[nodiscard]] Block* allocate();
        void                 deallocate(Block* block);
    };

    std::optional<std::reference_wrapper<Pool>> GetPool(size_t bytes);

    constexpr static std::array block_size = {
        // 4-increments
        8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96,

        // 32-increments
        128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640,

        // 64-increments
        704, 768, 832, 896, 960, 1024};

    std::array<Pool, block_size.size()> m_Pools;

    std::shared_ptr<spdlog::logger> m_Logger;
};

class MemoryManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    [[nodiscard]] void* Allocate(size_t bytes, size_t alignment = alignof(std::max_align_t));
    void                Free(void* p, size_t bytes, size_t alignment);

private:
    std::unique_ptr<MemoryPool> m_Pools;
};

}  // namespace Hitagi::Core
namespace Hitagi {
extern std::unique_ptr<Core::MemoryManager> g_MemoryManager;
}