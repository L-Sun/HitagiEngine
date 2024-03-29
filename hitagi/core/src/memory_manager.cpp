#include <hitagi/core/memory_manager.hpp>
#include <hitagi/utils/utils.hpp>

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

namespace hitagi::core {

MemoryPool::Page::Page(std::size_t size, std::size_t block_size)
    : size(size),
      alignment(std::align_val_t(0x1 << std::countr_zero(block_size))),
      data(static_cast<std::byte*>(operator new[](size, alignment))) {}

MemoryPool::Page::Page(Page&& other) noexcept
    : size(other.size), alignment(other.alignment), data(other.data) {
    other.data = nullptr;
}

auto MemoryPool::Page::operator=(Page&& rhs) noexcept -> Page& {
    if (this != &rhs) {
        data     = rhs.data;
        rhs.data = nullptr;
    }
    return *this;
}

MemoryPool::Page::~Page() {
    if (data != nullptr) {
        operator delete[](data, alignment);
    }
}

auto MemoryPool::Pool::new_page() -> Page& {
    Page page{page_size, block_size};

    std::size_t num_block = page_size / block_size;
    num_free_blocks += num_block;

    constexpr auto next_block = [](Block* block, std::size_t block_size) -> Block* {
        return reinterpret_cast<Block*>(reinterpret_cast<std::byte*>(block) + block_size);
    };

    auto p_block = page.GetHeadBlock();
    for (std::size_t i = 0; i < num_block - 1; i++) {
        p_block->next = next_block(p_block, block_size);
        p_block       = next_block(p_block, block_size);
    }
    p_block->next = nullptr;
    return pages.emplace_back(std::move(page));
}

auto MemoryPool::Pool::allocate() -> Block* {
    std::lock_guard lock(mutex);

    if (free_list == nullptr) {
        auto& page        = new_page();
        auto  first_block = page.GetHeadBlock();
        free_list         = first_block;
    }
    Block* result = free_list;
    free_list     = free_list->next;
    num_free_blocks--;

#ifdef HITAGI_DEBUG
    allocated_blocks.emplace(result);
#endif

    return result;
}

auto MemoryPool::Pool::deallocate(Block* block) -> void {
    std::lock_guard lock(mutex);

    block->next = free_list;
    free_list   = block;
    num_free_blocks++;

#ifdef HITAGI_DEBUG
    allocated_blocks.erase(block);
#endif
}

MemoryPool::MemoryPool(std::shared_ptr<spdlog::logger> logger)
    : m_Pools(InitPools(std::make_index_sequence<block_size.size()>{})),
      m_Logger(std::move(logger)) {
    std::size_t block_index = 0;
    for (std::size_t i = 0; i < pool_map.size(); i++) {
        if (i > block_size[block_index]) block_index++;
        pool_map[i] = block_index;
    }
    assert(block_index == block_size.size() - 1);
}

MemoryPool::~MemoryPool() {
    for (const auto& pool : m_Pools) {
        const auto num_block_per_page = pool.page_size / pool.block_size;
        const auto total_blocks       = pool.pages.size() * num_block_per_page;
        if (pool.num_free_blocks != total_blocks) {
            m_Logger->warn("MemoryPool[{} bytes]: {} bytes memory leak", pool.block_size, (total_blocks - pool.num_free_blocks) * pool.block_size);
        }
#ifdef HITAGI_DEBUG
        for (const auto allocated_block : pool.allocated_blocks) {
            m_Logger->warn("MemoryPool[{} bytes]: {}", pool.block_size, fmt::ptr(allocated_block));
        }
#endif
    }
}

auto MemoryPool::GetPool(std::size_t bytes) -> utils::optional_ref<Pool> {
    if (bytes > block_size.back()) return std::nullopt;
    return m_Pools.at(pool_map[bytes]);
}

auto MemoryPool::do_allocate(std::size_t bytes, std::size_t alignment) -> void* {
    void* result = nullptr;
    if (auto pool = GetPool(utils::align(bytes, alignment)); pool.has_value()) {
        result = pool->get().allocate();
        TracyAllocN(result, bytes, "Pool");
    } else {
        result = operator new[](bytes, std::align_val_t(alignment));
        TracyAllocN(result, bytes, "Built In");
    }
    return result;
}

void MemoryPool::do_deallocate(void* p, std::size_t bytes, std::size_t alignment) {
    if (auto pool = GetPool(utils::align(bytes, alignment)); pool.has_value()) {
        TracyFreeN(p, "Pool");
        pool->get().deallocate(reinterpret_cast<Block*>(p));
    } else {
        TracyFreeN(p, "Built In");
        operator delete[](reinterpret_cast<std::byte*>(p), std::align_val_t{alignment});
    }
}

MemoryManager::MemoryManager() : RuntimeModule("MemoryManager") {
    m_Logger->trace("Create Memory Pool...");
    m_Pools = std::make_unique<MemoryPool>(m_Logger);

    m_Logger->trace("Set pmr default resource");
    std::pmr::set_default_resource(m_Pools.get());
}

MemoryManager::~MemoryManager() {
    m_Logger->trace("Unset pmr default resource");
    std::pmr::set_default_resource(std::pmr::new_delete_resource());
}

}  // namespace hitagi::core