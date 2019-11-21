#pragma once
#include "DXHelper.hpp"
#include "d3dx12.h"
#include <dxgi1_6.h>
#include "d3dcompiler.h"

namespace d3dUtil {
ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device5* device, ID3D12GraphicsCommandList* cmdList,
    const void* initData, uint64_t byteSize,
    ComPtr<ID3D12Resource>& uploadBuffer);

template <typename T>
class UploadBuffer {
public:
    UploadBuffer(ID3D12Device5* device, unsigned elementeCount,
                 bool isConstantBuffer)
        : m_bIsConstantBuffer(isConstantBuffer) {
        m_szElement = isConstantBuffer ? ((sizeof(T) + 255) & ~255) : sizeof(T);

        auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resource_desc =
            CD3DX12_RESOURCE_DESC::Buffer(m_szElement * elementeCount);
        ThrowIfFailed(device->CreateCommittedResource(
            &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_pUploadBuffer)));

        ThrowIfFailed(m_pUploadBuffer->Map(
            0, nullptr, reinterpret_cast<void**>(&m_pMappedData)));
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
}  // namespace d3dUtil