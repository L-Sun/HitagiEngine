#pragma once
#include <unordered_map>
#include "d3dUtil.hpp"
#include "GpuBuffer.hpp"

namespace Hitagi::Graphics::DX12 {
template <typename TFrameConstant, typename TObjConstant>
class FrameResource {
public:
    FrameResource(ID3D12Device6* device, size_t numObjects) {
        m_FrameCBUploader = std::make_unique<d3dUtil::UploadBuffer<TFrameConstant>>(device, 1, true);
        m_ObjCBUploader   = std::make_unique<d3dUtil::UploadBuffer<TObjConstant>>(device, numObjects, true);
    }
    void UpdateFrameConstants(TFrameConstant frameConstants) { m_FrameCBUploader->CopyData(0, frameConstants); }
    void UpdateObjectConstants(size_t index, TObjConstant objectConstants) {
        m_ObjCBUploader->CopyData(index, objectConstants);
    }

    FrameResource(const FrameResource& rhs) = delete;
    FrameResource operator=(const FrameResource& rhs) = delete;
    ~FrameResource() {}

    uint64_t fence = static_cast<uint64_t>(D3D12_COMMAND_LIST_TYPE_DIRECT) << 56;

    std::unique_ptr<d3dUtil::UploadBuffer<TFrameConstant>> m_FrameCBUploader;
    std::unique_ptr<d3dUtil::UploadBuffer<TObjConstant>>   m_ObjCBUploader;
};
}  // namespace Hitagi::Graphics::DX12