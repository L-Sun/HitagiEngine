#include "Allocator.hpp"

#include "HitagiMath.hpp"

#include <cassert>
#include <cstring>

namespace Hitagi::Core {

Allocator::Allocator()

    = default;

Allocator::Allocator(size_t data_size, size_t page_size, size_t alignment)  {
    Reset(data_size, page_size, alignment);
}

Allocator::~Allocator() { FreeAll(); }

void Allocator::Reset(size_t data_size, size_t page_size, size_t alignment) {
    FreeAll();

    m_DataSize = data_size;
    m_PageSize = page_size;

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
        auto* new_page = reinterpret_cast<PageHeader*>(new uint8_t[m_PageSize]);
        new_page->next = nullptr;
        ++m_Pages;
        m_BLocks += m_BlocksPerPage;
        m_FreeBlocks += m_BlocksPerPage;

#if defined(_DEBUG)
        FillFreePage(new_page);
#endif  // DEBUG

        // push new page
        if (m_PageList) new_page->next = m_PageList;
        m_PageList = new_page;

        // the first block in page
        BlockHeader* block = new_page->Blocks();
        // fill block
        for (uint32_t i = 0; i < m_BlocksPerPage - 1; i++) {
            block->next = NextBlock(block);
            block       = NextBlock(block);
        }
        block->next = nullptr;  // the last block
        // push free block
        m_FreeList = new_page->Blocks();
    }

    BlockHeader* free_block = m_FreeList;
    m_FreeList             = m_FreeList->next;
    --m_FreeBlocks;

#if defined(_DEBUG)
    FillAllocatedBlock(free_block);
#endif  // DEBUG

    return reinterpret_cast<void*>(free_block);
}

void Allocator::Free(void* p) {
    auto* block = reinterpret_cast<BlockHeader*>(p);

#if defined(_DEBUG)
    FillFreeBlock(block);
#endif

    // push free block
    block->next = m_FreeList;
    m_FreeList  = block;
    ++m_FreeBlocks;
}

void Allocator::FreeAll() {
    PageHeader* page = m_PageList;

    while (page) {
        PageHeader* p = page;
        page           = page->next;
        delete[] reinterpret_cast<uint8_t*>(p);
    }

    m_PageList = nullptr;
    m_FreeList = nullptr;

    m_Pages      = 0;
    m_BLocks     = 0;
    m_FreeBlocks = 0;
}

#if defined(_DEBUG)

void Allocator::FillFreePage(PageHeader* page) {
    BlockHeader* p_block = page->Blocks();

    for (uint32_t i = 0; i < m_BlocksPerPage; i++) FillFreeBlock(p_block);
}

void Allocator::FillFreeBlock(BlockHeader* block) {
    std::memset(block, g_PatternFree, m_BlockSize - m_AlignmentSize);
    std::memset(reinterpret_cast<uint8_t*>(block) + m_BlockSize - m_AlignmentSize, g_PatternAlign, m_AlignmentSize);
}

void Allocator::FillAllocatedBlock(BlockHeader* block) {
    std::memset(block, g_PatternAlloc, m_BlockSize - m_AlignmentSize);
    std::memset(reinterpret_cast<uint8_t*>(block) + m_BlockSize - m_AlignmentSize, g_PatternAlign, m_AlignmentSize);
}

#endif  // DEBUG

BlockHeader* Allocator::NextBlock(BlockHeader* block) {
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<uint8_t*>(block) + m_BlockSize);
}
}  // namespace Hitagi::Core