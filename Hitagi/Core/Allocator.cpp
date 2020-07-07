#include "Allocator.hpp"

#include "HitagiMath.hpp"

#include <cassert>
#include <cstring>

namespace Hitagi::Core {

Allocator::Allocator()

    = default;

Allocator::Allocator(size_t dataSize, size_t pageSize, size_t alignment) : m_PageList(nullptr), m_FreeList(nullptr) {
    Reset(dataSize, pageSize, alignment);
}

Allocator::~Allocator() { FreeAll(); }

void Allocator::Reset(size_t dataSize, size_t pageSize, size_t alignment) {
    FreeAll();

    m_DataSize = dataSize;
    m_PageSize = pageSize;

    size_t minimal_size = (sizeof(BlockHeader) > m_DataSize) ? sizeof(BlockHeader) : m_DataSize;

#if defined(_DEBUG)
    assert(alignment > 0 && ((alignment & (alignment - 1))) == 0);
#endif

    m_BlockSize     = align(minimal_size, alignment);
    m_AlignmentSize = m_BlockSize - minimal_size;
    m_BlocksPerPage = (m_PageSize - sizeof(PageHeader)) / m_BlockSize;
}

void* Allocator::Allocate() {
    if (!m_FreeList) {
        auto* newPage = reinterpret_cast<PageHeader*>(new uint8_t[m_PageSize]);
        newPage->next = nullptr;
        ++m_Pages;
        m_BLocks += m_BlocksPerPage;
        m_FreeBlocks += m_BlocksPerPage;

#if defined(_DEBUG)
        FillFreePage(newPage);
#endif  // DEBUG

        // push new page
        if (m_PageList) newPage->next = m_PageList;
        m_PageList = newPage;

        // the first block in page
        BlockHeader* block = newPage->Blocks();
        // fill block
        for (uint32_t i = 0; i < m_BlocksPerPage - 1; i++) {
            block->next = NextBlock(block);
            block       = NextBlock(block);
        }
        block->next = nullptr;  // the last block
        // push free block
        m_FreeList = newPage->Blocks();
    }

    BlockHeader* freeBlock = m_FreeList;
    m_FreeList             = m_FreeList->next;
    --m_FreeBlocks;

#if defined(_DEBUG)
    FillAllocatedBlock(freeBlock);
#endif  // DEBUG

    return reinterpret_cast<void*>(freeBlock);
}

void Allocator::Free(void* p) {
    auto* block = reinterpret_cast<BlockHeader*>(p);

#if defined(_DEBUG)
    FillFreeBlock(block);
#endif  // DEBUG

    // push free block
    block->next = m_FreeList;
    m_FreeList  = block;
    ++m_FreeBlocks;
}

void Allocator::FreeAll() {
    PageHeader* page = m_PageList;

    while (page) {
        PageHeader* _p = page;
        page           = page->next;
        delete[] reinterpret_cast<uint8_t*>(_p);
    }

    m_PageList = nullptr;
    m_FreeList = nullptr;

    m_Pages      = 0;
    m_BLocks     = 0;
    m_FreeBlocks = 0;
}

#if defined(_DEBUG)

void Allocator::FillFreePage(PageHeader* page) {
    BlockHeader* pBlock = page->Blocks();

    for (uint32_t i = 0; i < m_BlocksPerPage; i++) FillFreeBlock(pBlock);
}

void Allocator::FillFreeBlock(BlockHeader* block) {
    std::memset(block, PATTERN_FREE, m_BlockSize - m_AlignmentSize);
    std::memset(reinterpret_cast<uint8_t*>(block) + m_BlockSize - m_AlignmentSize, PATTERN_ALIGN, m_AlignmentSize);
}

void Allocator::FillAllocatedBlock(BlockHeader* block) {
    std::memset(block, PATTERN_ALLOC, m_BlockSize - m_AlignmentSize);
    std::memset(reinterpret_cast<uint8_t*>(block) + m_BlockSize - m_AlignmentSize, PATTERN_ALIGN, m_AlignmentSize);
}

#endif  // DEBUG

BlockHeader* Allocator::NextBlock(BlockHeader* block) {
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<uint8_t*>(block) + m_BlockSize);
}
}  // namespace Hitagi::Core