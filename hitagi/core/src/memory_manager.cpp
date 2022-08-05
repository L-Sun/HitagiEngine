#include <hitagi/core/memory_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
core::MemoryManager* memory_manager = nullptr;
}

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

    auto p_block = page.get<Block>();
    for (std::size_t i = 0; i < num_block - 1; i++) {
        p_block->next = next_block(p_block, block_size);
        p_block       = next_block(p_block, block_size);
    }
    p_block->next = nullptr;
    return pages.emplace_back(std::move(page));
}

auto MemoryPool::Pool::allocate() -> Block* {
    std::lock_guard lock(m_Mutex);

    if (free_list == nullptr) {
        auto& page        = new_page();
        auto  first_block = page.get<Block>();
        free_list         = first_block;
    }
    Block* result = free_list;
    free_list     = free_list->next;
    num_free_blocks--;
    return result;
}

auto MemoryPool::Pool::deallocate(Block* block) -> void {
    std::lock_guard lock(m_Mutex);

    block->next = free_list;
    free_list   = block;
}

MemoryPool::MemoryPool() : m_Pools(InitPools(std::make_index_sequence<block_size.size()>{})) {
    std::size_t block_index = 0;
    for (std::size_t i = 0; i < pool_map.size(); i++) {
        if (i > block_size[block_index]) block_index++;
        pool_map[i] = block_index;
    }
    assert(block_index == block_size.size() - 1);
}

auto MemoryPool::GetPool(std::size_t bytes) -> std::optional<std::reference_wrapper<Pool>> {
    if (bytes > block_size.back()) return std::nullopt;
    return m_Pools.at(pool_map[bytes]);
}

auto MemoryPool::do_allocate(std::size_t bytes, std::size_t alignment) -> void* {
    if (auto pool = GetPool(align(bytes, alignment)); pool.has_value()) {
        auto result = pool->get().allocate();
        return result;
    }

    return operator new[](bytes, std::align_val_t(alignment));
}

auto MemoryPool::do_deallocate(void* p, std::size_t bytes, std::size_t alignment) -> void {
    if (auto pool = GetPool(align(bytes, alignment)); pool.has_value())
        return pool->get().deallocate(reinterpret_cast<Block*>(p));

    operator delete[](reinterpret_cast<std::byte*>(p), std::align_val_t{alignment});
}

bool MemoryManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MemoryManager");
    m_Logger->info("Initialize...");

    m_Logger->info("Initial Memory Pool...");
    m_Pools = std::make_unique<MemoryPool>();

    m_Logger->debug("Set pmr default resource");
    std::pmr::set_default_resource(m_Pools.get());
    return true;
}

void MemoryManager::Finalize() {
    m_Logger->debug("Unset pmr default resource");
    std::pmr::set_default_resource(std::pmr::new_delete_resource());
    m_Logger->info("Finalize.");
    m_Logger = nullptr;
}

void MemoryManager::Tick() {}

}  // namespace hitagi::core