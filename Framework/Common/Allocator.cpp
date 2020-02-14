#include <cassert>
#include <cstring>
#include "Allocator.hpp"

#ifndef ALIGN
#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))
#endif

using namespace My;

Allocator::Allocator()
    : m_pPageList(nullptr),
      m_pFreeList(nullptr),
      m_szDataSize(0),
      m_szPageSize(0),
      m_szAlignmentSize(0),
      m_szBlockSize(0),
      m_nBlocksPerPage(0) {}

Allocator::Allocator(size_t data_size, size_t page_size, size_t alignment)
    : m_pPageList(nullptr), m_pFreeList(nullptr) {
    Reset(data_size, page_size, alignment);
}

Allocator::~Allocator() { FreeAll(); }

void Allocator::Reset(size_t data_size, size_t page_size, size_t alignment) {
    FreeAll();

    m_szDataSize = data_size;
    m_szPageSize = page_size;

    size_t minimal_size = (sizeof(BlockHeader) > m_szDataSize) ? sizeof(BlockHeader) : m_szDataSize;

#if defined(DEBUG)
    assert(alignment > 0 && ((alignment & (alignment - 1))) == 0);
#endif

    m_szBlockSize     = ALIGN(minimal_size, alignment);
    m_szAlignmentSize = m_szBlockSize - minimal_size;
    m_nBlocksPerPage  = (m_szPageSize - sizeof(PageHeader)) / m_szBlockSize;
}

void* Allocator::Allocate() {
    if (!m_pFreeList) {
        PageHeader* pNewPage = reinterpret_cast<PageHeader*>(new uint8_t[m_szPageSize]);
        ++m_nPages;
        m_nBLocks += m_nBlocksPerPage;
        m_nFreeBlocks += m_nBlocksPerPage;

#if defined(DEBUG)
        FillFreePage(pNewPage);
#endif  // DEBUG

        // push new page
        if (m_pPageList) pNewPage->pNext = m_pPageList;
        m_pPageList = pNewPage;

        // the first block in page
        BlockHeader* pBlock = pNewPage->Blocks();
        // fill block
        for (uint32_t i = 0; i < m_nBlocksPerPage - 1; i++) {
            pBlock->pNext = NextBlock(pBlock);
            pBlock        = NextBlock(pBlock);
        }
        pBlock->pNext = nullptr;  // the last block
        // push free block
        m_pFreeList = pNewPage->Blocks();
    }

    BlockHeader* freeBlock = m_pFreeList;
    m_pFreeList            = m_pFreeList->pNext;
    --m_nFreeBlocks;

#if defined(DEBUG)
    FillAllocatedBlock(freeBlock);
#endif  // DEBUG

    return reinterpret_cast<void*>(freeBlock);
}

void Allocator::Free(void* p) {
    BlockHeader* block = reinterpret_cast<BlockHeader*>(p);

#if defined(DEBUG)
    FillFreeBlock(block);
#endif  // DEBUG

    // push free block
    block->pNext = m_pFreeList;
    m_pFreeList  = block;
    ++m_nFreeBlocks;
}

void Allocator::FreeAll() {
    PageHeader* pPage = m_pPageList;

    while (pPage) {
        PageHeader* _p = pPage;
        pPage          = pPage->pNext;
        delete[] reinterpret_cast<uint8_t*>(_p);
    }

    m_pPageList = nullptr;
    m_pFreeList = nullptr;

    m_nPages      = 0;
    m_nBLocks     = 0;
    m_nFreeBlocks = 0;
}

#if defined(DEBUG)

void Allocator::FillFreePage(PageHeader* pPage) {
    pPage->pNext        = nullptr;
    BlockHeader* pBlock = pPage->Blocks();

    for (uint32_t i = 0; i < m_nBlocksPerPage; i++) FillFreeBlock(pBlock);
}

void Allocator::FillFreeBlock(BlockHeader* pBlock) {
    std::memset(pBlock, PATTERN_FREE, m_szBlockSize - m_szAlignmentSize);
    std::memset(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize - m_szAlignmentSize, PATTERN_ALIGN,
                m_szAlignmentSize);
}

void Allocator::FillAllocatedBlock(BlockHeader* pBlock) {
    std::memset(pBlock, PATTERN_ALLOC, m_szBlockSize - m_szAlignmentSize);
    std::memset(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize - m_szAlignmentSize, PATTERN_ALIGN,
                m_szAlignmentSize);
}

#endif  // DEBUG

BlockHeader* Allocator::NextBlock(BlockHeader* pBlock) {
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<uint8_t*>(pBlock) + m_szBlockSize);
}