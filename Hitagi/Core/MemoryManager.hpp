#pragma once
#include "IRuntimeModule.hpp"

#include <memory_resource>

namespace Hitagi::Core {
constexpr std::uint64_t operator""_kb(unsigned long long val) { return static_cast<std::uint64_t>(val) << 10; }

class MemoryPool : public std::pmr::synchronized_pool_resource {
public:
    MemoryPool(const std::pmr::pool_options& options, std::shared_ptr<spdlog::logger> logger)
        : synchronized_pool_resource(options),
          m_Logger(logger) {}

private:
    [[nodiscard]] void* do_allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) final;
    void                do_deallocate(void* p, size_t bytes, size_t alignment) final;

    std::shared_ptr<spdlog::logger> m_Logger = nullptr;
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