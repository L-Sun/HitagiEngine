#pragma once
#include "D3Dpch.hpp"

namespace d3dUtil {
Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device5* device, ID3D12GraphicsCommandList* cmdList,
                                                           const void* initData, uint64_t byteSize,
                                                           Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

template <typename T>
class UploadBuffer {
public:
    UploadBuffer(ID3D12Device5* device, size_t elementeCount, bool isConstantBuffer)
        : m_ElementsCount(elementeCount), m_IsConstantBuffer(isConstantBuffer) {
        m_ElementSize = isConstantBuffer ? ((sizeof(T) + 255) & ~255) : sizeof(T);

        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(m_ElementSize * elementeCount);
        ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                      IID_PPV_ARGS(&m_UploadBuffer)));

        ThrowIfFailed(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer() {
        if (!m_MappedData) {
            m_UploadBuffer->Unmap(0, nullptr);
        }
        m_MappedData = nullptr;
    }

    ID3D12Resource* Resource() const { return m_UploadBuffer.Get(); }
    void            CopyData(int elementIndex, const T& data) {
        memcpy(&m_MappedData[elementIndex * m_ElementSize], &data, sizeof(T));
    }
    size_t GetCBByteSize() const { return m_ElementSize; }
    size_t GetElementCount() const { return m_ElementsCount; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_UploadBuffer;
    BYTE*                                  m_MappedData = nullptr;

    size_t m_ElementSize      = 0;
    size_t m_ElementsCount    = 0;
    bool   m_IsConstantBuffer = false;
};
}  // namespace d3dUtil