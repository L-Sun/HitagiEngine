#pragma once
#include "D3Dpch.hpp"
#include "RootSignature.hpp"

// Stage, commit or cpoy directly CPU visible descriptor to GPU visible descriptor.
namespace Hitagi::Graphics {
class CommandContext;

class DynamicDescriptorHeap {
public:
    DynamicDescriptorHeap(CommandContext& cmdContext, D3D12_DESCRIPTOR_HEAP_TYPE type,
                          uint32_t numDescriptorHeap = 1024);
    ~DynamicDescriptorHeap() { Reset(); }

    void StageDescriptor(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors,
                         const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorHandle);

    void CommitStagedDescriptors(
        ID3D12GraphicsCommandList5*                                                        cmdList,
        std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(ID3D12GraphicsCommandList5* cmdList,
                                               D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle);

    // Paser root signature to get information about the layout of descriptors.
    void ParseRootSignature(const RootSignature& rootSignature);

    void Reset();

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();
    uint32_t                                     StaleDescriptorCount() const;

    struct DescriptorTableCache {
        DescriptorTableCache() : baseHandle(nullptr), numDescriptors(0) {}
        void Reset() {
            baseHandle     = nullptr;
            numDescriptors = 0;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE* baseHandle;
        uint32_t                     numDescriptors;
    };

    CommandContext&            m_CommandContext;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    uint32_t                   m_NumDescriptorsPerHeap;
    uint32_t                   m_HandleIncrementSize;

    static const uint32_t m_MaxDescriptorTables = 32;

    std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_DescriptorHandleCache;
    DescriptorTableCache                           m_DescriptorTableCache[m_MaxDescriptorTables];

    // Each bit in the bit mask indicates which descriptor table is bound to the root signature.
    uint32_t m_DescriptorTableBitMask;
    // Each bit in the bit mask indicates a descriptor table in the root signature that has
    // changed since the last time the descriptors were copied.
    uint32_t m_StaleDescriptorTableBitMask;

    using DescriptorHeapPool = std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>;
    DescriptorHeapPool m_DescriptorHeapPool;
    DescriptorHeapPool m_AvailableDescriptorHeaps;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_CurrentDescriptorHeap;
    CD3DX12_GPU_DESCRIPTOR_HANDLE                m_CurrentGPUDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE                m_CurrentCPUDescriptorHandle;

    uint32_t m_NumFreeHandles;
};
}  // namespace Hitagi::Graphics