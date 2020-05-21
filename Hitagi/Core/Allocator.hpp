#pragma once
#include <cstdint>
#include <cstddef>

namespace Hitagi::Core {

struct BlockHeader {
    BlockHeader* next;
};

struct PageHeader {
    PageHeader*  next;
    BlockHeader* Blocks() { return reinterpret_cast<BlockHeader*>(this + 1); }
};

class Allocator {
public:
    static const uint8_t PATTERN_ALIGN = 0XFC;
    static const uint8_t PATTERN_ALLOC = 0XFD;
    static const uint8_t PATTERN_FREE  = 0XFE;

    Allocator();
    Allocator(size_t dataSize, size_t pageSize, size_t alignment);
    Allocator(const Allocator* clone);
    Allocator& operator=(const Allocator& rhs);
    ~Allocator();

    void Reset(size_t dataSize, size_t pageSize, size_t alignment);

    void* Allocate();
    void  Free(void* p);
    void  FreeAll();

private:
    void FillFreePage(PageHeader* page);
    void FillFreeBlock(BlockHeader* block);
    void FillAllocatedBlock(BlockHeader* block);

    BlockHeader* NextBlock(BlockHeader* block);
    PageHeader*  m_PageList = nullptr;
    BlockHeader* m_FreeList = nullptr;

    size_t   m_DataSize      = 0;
    size_t   m_PageSize      = 0;
    size_t   m_AlignmentSize = 0;
    size_t   m_BlockSize     = 0;
    uint32_t m_BlocksPerPage = 0;

    uint32_t m_Pages      = 0;
    uint32_t m_BLocks     = 0;
    uint32_t m_FreeBlocks = 0;
};
}  // namespace Hitagi::Core
