#pragma once
#include "RootSignature.hpp"
#include "DescriptorAllocator.hpp"

// Stage, commit or cpoy directly CPU visible descriptor to GPU visible descriptor.
namespace Hitagi::Graphics::backend::DX12 {
class CommandContext;

class DynamicDescriptorHeap {
    using FenceValue   = uint64_t;
    using FenceChecker = std::function<bool(FenceValue)>;

public:
    DynamicDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, FenceChecker&& checker);
    ~DynamicDescriptorHeap() { Reset(m_FenceValue); }

    void StageDescriptors(uint32_t rootParameterIndex, uint32_t offset, const std::vector<Descriptor>& descriptors);

    void CommitStagedDescriptors(
        CommandContext&                                                                    context,
        std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(CommandContext&             context,
                                               D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle);

    // Paser root signature to get information about the layout of descriptors.
    void ParseRootSignature(const RootSignature& rootSignature);

    void Reset(FenceValue fenceValue);

    static void Destroy() {
        std::lock_guard lock(kMutex);
        kDescriptorHeapPool       = {};
        kAvailableDescriptorHeaps = {};
    }

private:
    std::pair<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, FenceValue> RequestDescriptorHeap();

    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type);

    uint32_t StaleDescriptorCount() const;

    struct DescriptorTableCache {
        DescriptorTableCache() = default;
        void Reset() {
            baseHandle     = nullptr;
            numDescriptors = 0;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE* baseHandle     = nullptr;
        uint32_t                     numDescriptors = 0;
    };

    using DescriptorHeapPool = std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>;
    using AvailableHeapPool  = std::queue<std::pair<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, FenceValue>>;

    static const uint32_t                                                              kNumDescriptorsPerHeap = 1024;
    static const uint32_t                                                              kMaxDescriptorTables   = 32;
    inline static std::array<DescriptorHeapPool, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> kDescriptorHeapPool;
    inline static std::array<AvailableHeapPool, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>  kAvailableDescriptorHeaps;
    static std::mutex                                                                  kMutex;

    ID3D12Device*              m_Device;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    uint32_t                   m_HandleIncrementSize;
    FenceValue                 m_FenceValue;
    FenceChecker               m_FenceChecker;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>               m_DescriptorHandleCache;
    std::array<DescriptorTableCache, kMaxDescriptorTables> m_DescriptorTableCache;

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
}  // namespace Hitagi::Graphics::backend::DX12