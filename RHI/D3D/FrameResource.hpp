#pragma once
#include "d3dUtil.hpp"

template <typename TFrameConstant, typename TObjConstant>
class FrameResource {
public:
    FrameResource(ID3D12Device5* pDev, size_t objConstantCount,
                  std::shared_ptr<ID3D12PipelineState> pso)
        : m_pPSO(pso) {
        ThrowIfFailed(
            pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&m_pCommandAllocator)));
        ThrowIfFailed(pDev->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(),
            nullptr, IID_PPV_ARGS(&m_pCommandList)));
        ThrowIfFailed(m_pCommandList->Close());

        m_pFrameUploader =
            std::make_unique<d3dUtil::UploadBuffer<TFrameConstant>>(pDev, 1,
                                                                    true);
        m_pObjUploader = std::make_unique<d3dUtil::UploadBuffer<TObjConstant>>(
            pDev, objConstantCount, true);
    }
    void UpdateFrameConstants(TFrameConstant frameConstants) {
        m_pFrameUploader->CopyData(0, frameConstants);
    }
    void UpdateObjectConstants(size_t index, TObjConstant objectConstants) {
        m_pObjUploader->CopyData(index, objectConstants);
    }

    FrameResource(const FrameResource& rhs) = delete;
    FrameResource operator=(const FrameResource& rhs) = delete;
    ~FrameResource() {}

    uint64_t fence = 0;

    ComPtr<ID3D12CommandAllocator>       m_pCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList>    m_pCommandList;
    std::shared_ptr<ID3D12PipelineState> m_pPSO;

    std::unique_ptr<d3dUtil::UploadBuffer<TFrameConstant>> m_pFrameUploader;
    std::unique_ptr<d3dUtil::UploadBuffer<TObjConstant>>   m_pObjUploader;
};
