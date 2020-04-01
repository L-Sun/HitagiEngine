#pragma once
#include <queue>
#include <memory>
#include "GpuBuffer.hpp"

namespace Hitagi::Graphics {
class LinearAllocator {
    struct Allocation {
        GpuResource&              resource;
        size_t                    offset;
        size_t                    size;
        void*                     CpuPtr;
        D3D12_GPU_VIRTUAL_ADDRESS GpuPtr;
    };

    struct Page : public GpuResource {
        Page(size_t pageSize);
        ~Page();
        bool       HasSpace(size_t size, size_t alignment) const;
        Allocation Allocate(size_t size, size_t alignment);
        void       Reset();

        size_t m_PageSize;
        size_t m_Offset;
        void*  m_CpuPtr;
    };

public:
    LinearAllocator(size_t pageSize = 2_MB) : m_PageSize(pageSize) {}
    ~LinearAllocator();

    size_t     GetPageSize() const { return m_PageSize; };
    Allocation Allocate(size_t size, size_t alignment = 256);
    void       Reset();

private:
    std::shared_ptr<Page> RequestPage();

    size_t m_PageSize;

    std::vector<std::shared_ptr<Page>> m_PagePool;
    std::queue<std::shared_ptr<Page>>  m_AvailablePages;

    std::shared_ptr<Page> m_CurrPage;
};
}  // namespace Hitagi::Graphics