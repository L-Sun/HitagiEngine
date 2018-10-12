#include <cstdint>
#include <cstddef>

namespace My {

struct BlockHeader {
    BlockHeader* pNext;
};

struct PageHeader {
    PageHeader*  pNext;
    BlockHeader* Blocks() { return reinterpret_cast<BlockHeader*>(this + 1); }
};

class Allocator {
public:
    static const uint8_t PATTERN_ALIGN = 0XFC;
    static const uint8_t PATTERN_ALLOC = 0XFD;
    static const uint8_t PATTERN_FREE  = 0XFE;

    Allocator();
    Allocator(size_t data_size, size_t page_size, size_t alignment);
    Allocator(const Allocator* clone);
    Allocator& operator=(const Allocator& rhs);
    ~Allocator();

    void Reset(size_t data_size, size_t page_size, size_t alignment);

    void* Allocate();
    void  Free(void* p);
    void  FreeAll();

private:
#if defined(_DEBUG)
    void FillFreePage(PageHeader* pPage);
    void FillFreeBlock(BlockHeader* pBlock);
    void FillAllocatedBlock(BlockHeader* pBlock);
#endif  // _DEBUG

    BlockHeader* NextBlock(BlockHeader* pBlock);
    PageHeader*  m_pPageList;
    BlockHeader* m_pFreeList;

    size_t   m_szDataSize;
    size_t   m_szPageSize;
    size_t   m_szAlignmentSize;
    size_t   m_szBlockSize;
    uint32_t m_nBlocksPerPage;

    uint32_t m_nPages;
    uint32_t m_nBLocks;
    uint32_t m_nFreeBlocks;
};
}  // namespace My
