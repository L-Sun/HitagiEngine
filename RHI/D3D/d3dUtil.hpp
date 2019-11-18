#pragma once
#include "d3dx12.h"
#include "DXHelper.hpp"

namespace d3dUtil {
ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device5* device, ID3D12GraphicsCommandList* cmdList,
    const void* initData, uint64_t byteSize,
    ComPtr<ID3D12Resource>& uploadBuffer) {
    ComPtr<ID3D12Resource> defaultBuffer;
    // crete default heap
    auto default_heap_properties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto default_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &default_heap_properties, D3D12_HEAP_FLAG_NONE, &default_resource_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // create upload heap
    auto upload_heap_properties =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto upload_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &upload_heap_properties, D3D12_HEAP_FLAG_NONE, &upload_resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData                  = initData;
    subResourceData.RowPitch               = byteSize;
    subResourceData.SlicePitch             = subResourceData.RowPitch;

    auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier1);
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0,
                          0, 1, &subResourceData);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &barrier2);

    return defaultBuffer;
}

unsigned CalcConstantBufferByteSize(unsigned byteSize) {
    return (byteSize + 255) & ~255;
}

template <typename T>
class UploadBuffer {
public:
    UploadBuffer(ID3D12Device5* device, unsigned elementeCount,
                 bool isConstantBuffer)
        : m_bIsConstantBuffer(isConstantBuffer) {
        m_szElement = isConstantBuffer ? CalcConstantBufferByteSize(sizeof(T))
                                       : sizeof(T);

        auto heap_desc = CD3DX12_HEAP_DESC(D3D12_HEAP_TYPE_UPLOAD);
        auto resource_desc =
            CD3DX12_RESOURCE_DESC::Buffer(m_szElement * elementeCount);
        ThrowIfFailed(device->CreateCommittedResource(
            &heap_desc, D3D12_HEAP_FLAG_NONE, &resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_pUploadBuffer)));

        ThrowIfFailed(m_pUploadBuffer->Map(
            0, D3D12_RANGE, reinterpret_cast<void**>(&m_pMappedData)));
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer() {
        if (!m_pMappedData) {
            m_pUploadBuffer->Unmap(0, nullptr);
        }
        m_pMappedData = nullptr;
    }

    ID3D12Resource* Resource() const { return m_pUploadBuffer.Get(); }
    void            CopyData(int elementIndex, const T& data) {
        memcpy(&m_pMappedData[elementIndex * m_szElement], &data, sizeof(T));
    }

private:
    ComPtr<ID3D12Resource> m_pUploadBuffer;
    BYTE*                  m_pMappedData = nullptr;

    unsigned m_szElement         = 0;
    bool     m_bIsConstantBuffer = false;
};

};  // namespace d3dUtil