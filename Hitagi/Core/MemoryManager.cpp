#include "MemoryManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "portable.hpp"

namespace Hitagi::Core {

static const uint32_t kBlockSizes[] = {
    // 4-increments
    4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96,

    // 32-increments
    128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640,

    // 64-increments
    704, 768, 832, 896, 960, 1024};

static const uint32_t kPageSize  = 8192;
static const uint32_t kAlignment = 4;

static const uint32_t kNumBlockSizes = sizeof(kBlockSizes) / sizeof(kBlockSizes[0]);

static const uint32_t kMaxBlockSize = kBlockSizes[kNumBlockSizes - 1];
size_t*               MemoryManager::m_BlockSizeLookup;
Allocator*            MemoryManager::m_Allocators;

int MemoryManager::Initialize() {
    if (m_Initialized) return 0;

    m_Logger = spdlog::stdout_color_mt("MemoryManager");
    m_Logger->info("Initialize...");

    m_BlockSizeLookup = new size_t[kMaxBlockSize + 1];

    for (size_t i = 0, j = 0; i <= kMaxBlockSize; i++) {
        if (i > kBlockSizes[j]) ++j;
        m_BlockSizeLookup[i] = j;
    }

    m_Allocators = new Allocator[kNumBlockSizes];
    for (size_t i = 0; i < kNumBlockSizes; i++) m_Allocators[i].Reset(kBlockSizes[i], kPageSize, kAlignment);

    m_Initialized = true;
    return 0;
}

void MemoryManager::Finalize() {
    for (size_t i = 0; i < kNumBlockSizes; i++) {
        m_Allocators[i].FreeAll();
    }

    delete[] m_Allocators;
    delete[] m_BlockSizeLookup;

    m_Logger->info("Finalize.");
    m_Logger      = nullptr;
    m_Initialized = false;
}

void MemoryManager::Tick() {}

Allocator* MemoryManager::LookUpAllocator(size_t size) {
    return size <= kMaxBlockSize ? (m_Allocators + m_BlockSizeLookup[size]) : nullptr;
}

void* MemoryManager::Allocate(size_t size) {
    Allocator* allocator = LookUpAllocator(size);
    if (allocator)
        return allocator->Allocate();
    else
        return new uint8_t[size];
}

void* MemoryManager::Allocate(size_t size, size_t alignment) {
    uint8_t* p;
    size = align(size, alignment);

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