#include "MemoryManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>

#include "HitagiMath.hpp"

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager = std::make_unique<Core::MemoryManager>();
}

namespace Hitagi::Core {

constexpr std::array block_size = {
    // 4-increments
    4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96,

    // 32-increments
    128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640,

    // 64-increments
    704, 768, 832, 896, 960, 1024};

constexpr uint32_t g_KPageSize  = 8192;
constexpr uint32_t g_KAlignment = 4;

constexpr uint32_t g_KMaxBlockSize = block_size[block_size.size() - 1];

int MemoryManager::Initialize() {
    if (m_Initialized) return 0;

    m_Logger = spdlog::stdout_color_mt("MemoryManager");
    m_Logger->info("Initialize...");

    sm_BlockSizeLookup = new size_t[g_KMaxBlockSize + 1];

    for (size_t i = 0, j = 0; i <= g_KMaxBlockSize; i++) {
        if (i > block_size[j]) ++j;
        sm_BlockSizeLookup[i] = j;
    }

    sm_Allocators = new Allocator[block_size.size()];
    for (size_t i = 0; i < block_size.size(); i++) sm_Allocators[i].Reset(block_size[i], g_KPageSize, g_KAlignment);

    m_Initialized = true;
    return 0;
}

void MemoryManager::Finalize() {
    for (size_t i = 0; i < block_size.size(); i++) {
        sm_Allocators[i].FreeAll();
    }

    delete[] sm_Allocators;
    delete[] sm_BlockSizeLookup;

    m_Logger->info("Finalize.");
    m_Logger      = nullptr;
    m_Initialized = false;
}

void MemoryManager::Tick() {}

Allocator* MemoryManager::LookUpAllocator(size_t size) {
    return size <= g_KMaxBlockSize ? (sm_Allocators + sm_BlockSizeLookup[size]) : nullptr;
}

void* MemoryManager::Allocate(size_t size) {
    Allocator* allocator = LookUpAllocator(size);
    if (allocator)
        return allocator->Allocate();
    else
        return new uint8_t[size];
}

void* MemoryManager::Allocate(size_t size, size_t alignment) {
    uint8_t* p = nullptr;
    size       = align(size, alignment);

    Allocator* allocator = LookUpAllocator(size);
    if (allocator)
        p = reinterpret_cast<uint8_t*>(allocator->Allocate());
    else
        p = new uint8_t[size];

    p = reinterpret_cast<uint8_t*>(align(reinterpret_cast<size_t>(p), alignment));

    return p;
}

void MemoryManager::Free(void* p, size_t size) {
    // fix free repeatedly
    if (m_Initialized == false) return;
    Allocator* allocator = LookUpAllocator(size);
    if (allocator)
        allocator->Free(p);
    else
        delete[] reinterpret_cast<uint8_t*>(p);
}
}  // namespace Hitagi::Core