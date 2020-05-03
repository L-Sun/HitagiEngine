#pragma once
#include "RootSignature.hpp"

// Stage, commit or cpoy directly CPU visible descriptor to GPU visible descriptor.
namespace Hitagi::Graphics::DX12 {
class CommandContext;

class DynamicDescriptorHeap {
public:
    DynamicDescriptorHeap(CommandContext& cmdContext, D3D12_DESCRIPTOR_HEAP_TYPE type);
    ~DynamicDescriptorHeap() { Reset(m_FenceValue); }

    void StageDescriptor(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors,
                         const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorHandle);

    void CommitStagedDescriptors(
        ID3D12GraphicsCommandList5*                                                        cmdList,
        std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(ID3D12GraphicsCommandList5* cmdList,
                                               D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle);

    // Paser root signature to get information about the layout of descriptors.
    void ParseRootSignature(const RootSignature& rootSignature);

    void Reset(uint64_t fenceValue);

private:
    std::pair<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, uint64_t> RequestDescriptorHeap();

    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

    uint32_t StaleDescriptorCount() const;

    struct DescriptorTableCache {
        DescriptorTableCache() : baseHandle(nullptr), numDescriptors(0) {}
        void Reset() {
            baseHandle     = nullptr;
            numDescriptors = 0;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE* baseHandle;
        uint32_t                     numDescriptors;
    };

    using DescriptorHeapPool = std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>;
    using AvailableHeapPool  = std::queue<std::pair<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, uint64_t>>;

    static const uint32_t     kNumDescriptorsPerHeap = 1024;
    static const uint32_t     kMaxDescriptorTables   = 32;
    static DescriptorHeapPool kDescriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    static AvailableHeapPool  kAvailableDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    static std::mutex         kMutex;

    CommandContext&            m_CommandContext;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    uint32_t                   m_HandleIncrementSize;
    uint64_t                   m_FenceValue;

    std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_DescriptorHandleCache;
    DescriptorTableCache                           m_DescriptorTableCache[kMaxDescriptorTables];

    // Each bit in the bit mask indicates which descriptor table is bound to the root signature.
    uint32_t m_DescriptorTableBitMask;
    // Each bit in the bit mask indicates a descriptor table in the root signature that has
    // changed since the last time the descriptors were copied.
    uint32_t m_StaleDescriptorTableBitMask;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_CurrentDescriptorHeap;
    CD3DX12_GPU_DESCRIPTOR_HANDLE                m_CurrentGPUDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE                m_CurrentCPUDescriptorHandle;

    uint32_t m_NumFreeHandles;
};
}  // namespace Hitagi::Graphics::DX12