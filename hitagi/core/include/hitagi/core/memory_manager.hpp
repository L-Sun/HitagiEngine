#pragma once
#include "runtime_module.hpp"

#include <list>
#include <array>
#include <memory_resource>
#include <cassert>
#include <type_traits>

namespace hitagi::core {

inline const size_t align(size_t x, size_t a) {
    assert(((a - 1) & a) == 0 && "alignment is not a power of two");
    return (x + a - 1) & ~(a - 1);
}

constexpr std::size_t operator""_kB(unsigned long long val) { return val << 10; }

class MemoryPool : public std::pmr::memory_resource {
public:
    MemoryPool();
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

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
        Page(const Page&) = delete;
        Page& operator=(const Page&) = delete;
        Page(Page&&) noexcept;
        Page& operator=(Page&&) noexcept;
        ~Page();

        template <typename T>
        inline auto get() noexcept { return reinterpret_cast<std::remove_cv_t<T>*>(data); }

    private:
        const std::size_t      size;
        const std::align_val_t alignment;
        std::byte*             data;
    };

    struct Pool {
        std::mutex m_Mutex{};

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

    std::optional<std::reference_wrapper<Pool>> GetPool(std::size_t bytes);

    std::array<Pool, block_size.size()> m_Pools;

    template <std::size_t... Ns>
    constexpr auto InitPools(std::index_sequence<Ns...>) {
        return std::array{(Pool{.block_size = block_size.at(Ns)})...};
    }
};

class MemoryManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    [[nodiscard]] void* Allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

    template <typename T>
    [[nodiscard]] T* Allocate(std::size_t count, std::size_t alignment = alignof(T)) {
        return static_cast<T*>(Allocate(count * sizeof(T), alignment));
    }

    void Free(void* p, std::size_t bytes, std::size_t alignment);

private:
    std::unique_ptr<MemoryPool> m_Pools;
};

}  // namespace hitagi::core
namespace hitagi {
extern std::unique_ptr<core::MemoryManager> g_MemoryManager;
}