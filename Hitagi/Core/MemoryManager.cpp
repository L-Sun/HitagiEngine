#include "MemoryManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>

#include "HitagiMath.hpp"

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager = std::make_unique<Core::MemoryManager>();
}

namespace Hitagi::Core {

void* MemoryPool::do_allocate(size_t bytes, size_t alignment) {
    m_Logger->debug("Allocate {} byte(s), align: {} byte(s)", bytes, alignment);
    return synchronized_pool_resource::do_allocate(bytes, alignment);
}

void MemoryPool::do_deallocate(void* p, size_t bytes, size_t alignment) {
    m_Logger->debug("Deallocate {} byte(s), align: {} byte(s)", bytes, alignment);
    synchronized_pool_resource::do_deallocate(p, bytes, alignment);
}

int MemoryManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MemoryManager");
    m_Logger->info("Initialize...");

    m_Logger->info("Initial Memory Pool...");
    m_Pools = std::make_unique<MemoryPool>(
        std::pmr::pool_options{
            .max_blocks_per_chunk        = 4096,
            .largest_required_pool_block = 8_kb},
        m_Logger);

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