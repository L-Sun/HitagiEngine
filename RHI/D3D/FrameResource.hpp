#pragma once
#include "d3dUtil.hpp"
#include <unordered_map>

template <typename TFrameConstant, typename TObjConstant>
class FrameResource {
public:
    FrameResource(ID3D12Device5* pDev, size_t objCount) {
        ThrowIfFailed(
            pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&m_pCommandAllocator)));
        ThrowIfFailed(pDev->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(),
            nullptr, IID_PPV_ARGS(&m_pCommandList)));
        ThrowIfFailed(m_pCommandList->Close());

        m_pFrameCBUploader =
            std::make_unique<d3dUtil::UploadBuffer<TFrameConstant>>(pDev, 1,
                                                                    true);
        m_pObjCBUploader =
            std::make_unique<d3dUtil::UploadBuffer<TObjConstant>>(
                pDev, objCount, true);
    }
    void UpdateFrameConstants(TFrameConstant frameConstants) {
        m_pFrameCBUploader->CopyData(0, frameConstants);
    }
    void UpdateObjectConstants(size_t index, TObjConstant objectConstants) {
        m_pObjCBUploader->CopyData(index, objectConstants);
    }

    void UpdateTexture(std::string_view name, void* data) {
        if (m_textures.find(name) != m_textures.end()) {
            ComPtr<ID3D12Resource>&    texture    = m_textures[name].texture;
            ComPtr<ID3D12Resource>&    uploadHeap = m_textures[name].uploadHeap;
            const D3D12_RESOURCE_DESC& desc =
                m_textures[name].texture->GetDesc();
            D3D12_SUBRESOURCE_DATA textureData;
            textureData.pData      = data;
            textureData.RowPitch   = desc.Width * 4;
            textureData.SlicePitch = textureData.RowPitch * desc.Height;

            UpdateSubresources(m_pCommandList.Get(), texture.Get(),
                               uploadHeap.Get(), 0, 0, 1, &textureData);
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_pCommandList->ResourceBarrier(1, &barrier);
        }
    }

    FrameResource(const FrameResource& rhs) = delete;
    FrameResource operator=(const FrameResource& rhs) = delete;
    ~FrameResource() {}

    uint64_t fence = 0;

    ComPtr<ID3D12CommandAllocator>    m_pCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

    std::unique_ptr<d3dUtil::UploadBuffer<TFrameConstant>> m_pFrameCBUploader;
    std::unique_ptr<d3dUtil::UploadBuffer<TObjConstant>>   m_pObjCBUploader;
};
