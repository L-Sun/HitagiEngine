#pragma once
#include "LinearAllocator.hpp"

namespace Hitagi::Graphics::DX12 {
template <typename TFrameConstant, typename TObjConstant>
class FrameResource {
public:
    FrameResource(LinearAllocator& uploadBufferAlloc, size_t numObjects)
        : m_NumObjects(numObjects),
          m_FrameConstantBuffer(uploadBufferAlloc.Allocate<TFrameConstant>(1)),
          m_ObjectConstantBuffer(uploadBufferAlloc.Allocate<TObjConstant>(numObjects)) {
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
    ~FrameResource() {}

    uint64_t                    fence = static_cast<uint64_t>(D3D12_COMMAND_LIST_TYPE_DIRECT) << 56;
    size_t                      m_NumObjects;
    size_t                      m_SizeObject;
    LinearAllocator::Allocation m_FrameConstantBuffer, m_ObjectConstantBuffer;
};

}  // namespace Hitagi::Graphics::DX12