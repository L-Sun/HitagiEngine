#pragma once
#include "GpuBuffer.hpp"

namespace Hitagi::Graphics::DX12 {
class LinearAllocator {
public:
    struct Allocation {
        GpuResource&              resource;
        size_t                    offset;
        size_t                    size;
        void*                     CpuPtr;
        D3D12_GPU_VIRTUAL_ADDRESS GpuPtr;
    };

private:
    struct Page : public GpuResource {
        Page(ID3D12Device6* device, size_t pageSize);
        ~Page();
        bool       HasSpace(size_t size, size_t alignment) const;
        Allocation Allocate(size_t size, size_t alignment);
        void       Reset();

        ID3D12Device6* m_Device;
        size_t         m_PageSize;
        size_t         m_Offset;
        void*          m_CpuPtr;
    };

public:
    LinearAllocator() : m_Device(nullptr), m_PageSize(0), m_Initialized(false) {}
    LinearAllocator(ID3D12Device6* device, size_t pageSize = 10_MB)
        : m_Device(device), m_PageSize(pageSize), m_Initialized(true) {}
    ~LinearAllocator();

    void       Initalize(ID3D12Device6* device, size_t pageSize = 10_MB);
    size_t     GetPageSize() const { return m_PageSize; };
    Allocation Allocate(size_t size, size_t alignment = 256);
    template <typename T>
    Allocation Allocate(size_t count, size_t alignment = 256) {
        size_t size = (sizeof(T) + alignment - 1) & ~(alignment - 1);
        return Allocate(size * count, alignment);
    }
    void Reset();

private:
    bool                  m_Initialized = false;
    ID3D12Device6*        m_Device;
    std::shared_ptr<Page> RequestPage();

    size_t m_PageSize;

    std::vector<std::shared_ptr<Page>> m_PagePool;
    std::queue<std::shared_ptr<Page>>  m_AvailablePages;

    std::shared_ptr<Page> m_CurrPage;
};
}  // namespace Hitagi::Graphics::DX12