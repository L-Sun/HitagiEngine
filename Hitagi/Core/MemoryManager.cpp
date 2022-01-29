#include "MemoryManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>

#include "HitagiMath.hpp"

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager = std::make_unique<Core::MemoryManager>();
}

namespace Hitagi::Core {

auto MemoryPool::Pool::new_page() -> Page& {
    auto page = std::unique_ptr<std::byte[]>(
        new (std::align_val_t(block_size)) std::byte[page_size]);

    size_t num_block = page_size / block_size;
    num_free_blocks += num_block;

    constexpr auto next_block = [](Block* block, size_t block_size) -> Block* {
        return reinterpret_cast<Block*>(reinterpret_cast<std::byte*>(block) + block_size);
    };

    auto p_block = reinterpret_cast<Block*>(page.get());
    for (size_t i = 0; i < num_block - 1; i++) {
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
        auto  first_block = reinterpret_cast<Block*>(page.get());
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

MemoryPool::MemoryPool(std::shared_ptr<spdlog::logger> logger)
    : m_Logger(logger) {
    m_Logger->set_level(spdlog::level::debug);
    for (size_t i = 0; i < block_size.size(); i++) {
        m_Pools.at(i).block_size = block_size.at(i);
        m_Pools.at(i).page_size  = 8_kB;
    }
}

auto MemoryPool::GetPool(size_t bytes) -> std::optional<std::reference_wrapper<Pool>> {
    auto   iter  = std::lower_bound(std::begin(block_size), std::end(block_size), bytes);
    size_t index = std::distance(std::begin(block_size), iter);
    if (index == m_Pools.size()) {
        return std::nullopt;
    }
    return m_Pools.at(index);
}

auto MemoryPool::do_allocate(size_t bytes, size_t alignment) -> void* {
    if (auto pool = GetPool(bytes); pool.has_value()) {
        auto result = pool->get().allocate();
        m_Logger->debug("Pool({} bytes): num_pages: {}, free_blocks: {}",
                        pool->get().block_size,
                        pool->get().pages.size(),
                        pool->get().num_free_blocks);
        return result;
    }

    m_Logger->debug("using new/delete to allocate {} bytes", bytes);
    return new (std::align_val_t(alignment)) std::byte[bytes];
}

auto MemoryPool::do_deallocate(void* p, size_t bytes, size_t alignment) -> void {
    if (auto pool = GetPool(bytes); pool.has_value()) return pool->get().deallocate(reinterpret_cast<Block*>(p));

    delete[] reinterpret_cast<std::byte*>(p);
}

int MemoryManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MemoryManager");
    m_Logger->info("Initialize...");

    m_Logger->info("Initial Memory Pool...");
    m_Pools = std::make_unique<MemoryPool>(m_Logger);

    m_Logger->debug("Set pmr default resource");
    std::pmr::set_default_resource(m_Pools.get());
    return 0;
}

void MemoryManager::Finalize() {
    m_Logger->debug("Unset pmr default resource");
    std::pmr::set_default_resource(std::pmr::new_delete_resource());
    m_Logger->info("Finalize.");
    m_Logger = nullptr;
}

void MemoryManager::Tick() {}

void* MemoryManager::Allocate(size_t bytes, size_t alignment) {
    return m_Pools->allocate(bytes, alignment);
}

void MemoryManager::Free(void* p, size_t bytes, size_t alignment) {
    m_Pools->deallocate(p, bytes, alignment);
}

}  // namespace Hitagi::Core