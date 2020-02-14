#include "MemoryManager.hpp"
#include <malloc.h>

using namespace My;

#ifndef ALIGN
#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))
#endif

namespace My {
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
size_t*               MemoryManager::m_pBlockSizeLookup;
Allocator*            MemoryManager::m_pAllocators;
}  // namespace My

int MemoryManager::Initialize() {
    m_bInitialized = false;
    if (!m_bInitialized) {
        m_pBlockSizeLookup = new size_t[kMaxBlockSize + 1];

        for (size_t i = 0, j = 0; i <= kMaxBlockSize; i++) {
            if (i > kBlockSizes[j]) ++j;
            m_pBlockSizeLookup[i] = j;
        }

        m_pAllocators = new Allocator[kNumBlockSizes];
        for (size_t i = 0; i < kNumBlockSizes; i++) m_pAllocators[i].Reset(kBlockSizes[i], kPageSize, kAlignment);

        m_bInitialized = true;
    }
    return 0;
}

void MemoryManager::Finalize() {
    for (size_t i = 0; i < kNumBlockSizes; i++) {
        m_pAllocators[i].FreeAll();
    }

    delete[] m_pAllocators;
    delete[] m_pBlockSizeLookup;
    m_bInitialized = false;
}

void MemoryManager::Tick() {}

Allocator* MemoryManager::LookUpAllocator(size_t size) {
    return size <= kMaxBlockSize ? (m_pAllocators + m_pBlockSizeLookup[size]) : nullptr;
}

void* MemoryManager::Allocate(size_t size) {
    Allocator* pAlloc = LookUpAllocator(size);
    if (pAlloc)
        return pAlloc->Allocate();
    else
        return new uint8_t[size];
}

void* MemoryManager::Allocate(size_t size, size_t alignment) {
    uint8_t* p;
    size = ALIGN(size, alignment);

    Allocator* pAlloc = LookUpAllocator(size);
    if (pAlloc)
        p = reinterpret_cast<uint8_t*>(pAlloc->Allocate());
    else
        p = new uint8_t[size];

    p = reinterpret_cast<uint8_t*>(ALIGN(reinterpret_cast<size_t>(p), alignment));

    return p;
}

void MemoryManager::Free(void* p, size_t size) {
    // fix free repeatedly
    if (m_bInitialized == false) return;
    Allocator* pAlloc = LookUpAllocator(size);
    if (pAlloc)
        pAlloc->Free(p);
    else
        delete[] reinterpret_cast<uint8_t*>(p);
}