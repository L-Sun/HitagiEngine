#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/utils/utils.hpp>
#include <hitagi/utils/types.hpp>

#include <list>
#include <array>
#include <memory_resource>

constexpr std::size_t operator""_kB(unsigned long long val) { return val << 10; }

namespace hitagi::core {
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
        std::mutex      mutex{};
        std::list<Page> pages{};
        Block*          free_list       = nullptr;
        std::size_t     page_size       = 8_kB;
        std::size_t     block_size      = 0;
        std::size_t     num_free_blocks = 0;

        Page&                new_page();
        [[nodiscard]] Block* allocate();
        void                 deallocate(Block* block);
    };

    constexpr static std::array block_size = {
        // 4-increments
        8u, 12u, 16u, 20u, 24u, 28u, 32u, 36u, 40u, 44u, 48u, 52u, 56u, 60u, 64u, 68u, 72u, 76u, 80u, 84u, 88u, 92u, 96u,

        // 32-increments
        128u, 160u, 192u, 224u, 256u, 288u, 320u, 352u, 384u, 416u, 448u, 480u, 512u, 544u, 576u, 608u, 640u,

        // 64-increments
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

}  // namespace hitagi::core

namespace hitagi {
extern core::MemoryManager* memory_manager;
}  // namespace hitagi