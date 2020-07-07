#pragma once
#include "LinearAllocator.hpp"

namespace Hitagi::Graphics::backend::DX12 {
class FrameResourceLinearAllocator {
protected:
    inline static LinearAllocator sm_CpuLineraAllocator = LinearAllocator(LinearAllocatorType::CPU_WRITABLE);
};

template <typename TFrameConstant, typename TObjConstant>
class FrameResource : public FrameResourceLinearAllocator {
public:
    FrameResource(size_t numObjects)
        : FrameResourceLinearAllocator(),
          m_NumObjects(numObjects),
          m_FrameConstantBuffer(FrameResourceLinearAllocator::sm_CpuLineraAllocator.Allocate<TFrameConstant>(1)),
          m_ObjectConstantBuffer(
              FrameResourceLinearAllocator::sm_CpuLineraAllocator.Allocate<TObjConstant>(numObjects)) {
        m_SizeObject = m_ObjectConstantBuffer.size / m_NumObjects;
    }
    void UpdateFrameConstants(TFrameConstant frameConstants) {
        std::memcpy(m_FrameConstantBuffer.CpuPtr, &frameConstants, sizeof(TFrameConstant));
    }
    void UpdateObjectConstants(size_t index, TObjConstant objectConstants) {
        void* dest = &static_cast<uint8_t*>(m_ObjectConstantBuffer.CpuPtr)[index * m_SizeObject];
        std::memcpy(dest, &objectConstants, sizeof(objectConstants));
    }
    inline size_t FrameConstantSize() const { return m_FrameConstantBuffer.size; }
    inline size_t ObjectSize() const { return m_SizeObject; }

    FrameResource(const FrameResource& rhs) = delete;
    FrameResource operator=(const FrameResource& rhs) = delete;
    ~FrameResource()                                  = default;

    uint64_t         fence = static_cast<uint64_t>(D3D12_COMMAND_LIST_TYPE_DIRECT) << 56;
    size_t           m_NumObjects;
    size_t           m_SizeObject;
    LinearAllocation m_FrameConstantBuffer, m_ObjectConstantBuffer;
};

}  // namespace Hitagi::Graphics::backend::DX12