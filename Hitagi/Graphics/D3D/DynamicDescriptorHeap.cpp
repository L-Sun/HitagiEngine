#include "DynamicDescriptorHeap.hpp"
#include "D3D12GraphicsManager.hpp"

namespace Hitagi::Graphics {
DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext& cmdContext, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             uint32_t numDescriptorHeap)
    : m_CommandContext(cmdContext),
      m_Type(type),
      m_NumDescriptorsPerHeap(numDescriptorHeap),
      m_DescriptorTableBitMask(0),
      m_StaleDescriptorTableBitMask(0),
      m_CurrentCPUDescriptorHandle(D3D12_DEFAULT),
      m_CurrentGPUDescriptorHandle(D3D12_DEFAULT),
      m_NumFreeHandles(0) {
    auto device             = static_cast<D3D12GraphicsManager*>(g_GraphicsManager.get())->m_Device;
    m_HandleIncrementSize   = device->GetDescriptorHandleIncrementSize(m_Type);
    m_DescriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_NumDescriptorsPerHeap);
}

void DynamicDescriptorHeap::Reset() {
    m_AvailableDescriptorHeaps = m_DescriptorHeapPool;
    m_CurrentDescriptorHeap.Reset();
    m_CurrentCPUDescriptorHandle  = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_CurrentGPUDescriptorHandle  = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_NumFreeHandles              = 0;
    m_DescriptorTableBitMask      = 0;
    m_StaleDescriptorTableBitMask = 0;

    // Reset the table cache
    for (int i = 0; i < m_MaxDescriptorTables; ++i) m_DescriptorTableCache[i].Reset();
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature) {
    m_StaleDescriptorTableBitMask   = 0;
    const auto& rootSignatureDesc   = rootSignature.GetRootSignatureDesc();
    m_DescriptorTableBitMask        = rootSignature.GetDescriptorTableBitMask(m_Type);
    uint32_t descriptorTableBitMask = m_DescriptorTableBitMask;

    uint32_t currentOffset = 0;
    DWORD    rootIndex;
    while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters) {
        uint32_t numDescriptors = rootSignature.GetNumDescriptors(rootIndex);

        auto& tableCache          = m_DescriptorTableCache[rootIndex];
        tableCache.baseHandle     = &m_DescriptorHandleCache[currentOffset];
        tableCache.numDescriptors = numDescriptors;

        currentOffset += numDescriptors;
        // filp current bit
        descriptorTableBitMask ^= (1 << rootIndex);
    }
    assert(currentOffset <= m_NumDescriptorsPerHeap &&
           "The root signature requires more than the maximum number of descriptors per descriptor "
           "heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptor(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors,
                                            const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorHandle) {
    if (numDescriptors > m_NumDescriptorsPerHeap || rootParameterIndex >= m_MaxDescriptorTables) throw std::bad_alloc();

    auto& tableCache = m_DescriptorTableCache[rootParameterIndex];
    if (offset + numDescriptors > tableCache.numDescriptors)
        throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");

    D3D12_CPU_DESCRIPTOR_HANDLE* handle = tableCache.baseHandle + offset;
    for (uint32_t i = 0; i < numDescriptors; i++) {
        handle[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptorHandle, i, m_HandleIncrementSize);
    }
    m_StaleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

uint32_t DynamicDescriptorHeap::StaleDescriptorCount() const {
    uint32_t numStaleDescriptors = 0;
    DWORD    i;
    DWORD    staleDescriptorsBitMask = m_StaleDescriptorTableBitMask;
    while (_BitScanForward(&i, staleDescriptorsBitMask)) {
        numStaleDescriptors += m_DescriptorTableCache[i].numDescriptors;
        staleDescriptorsBitMask ^= (1 << i);
    }

    return numStaleDescriptors;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap() {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (!m_AvailableDescriptorHeaps.empty()) {
        descriptorHeap = m_AvailableDescriptorHeaps.front();
        m_AvailableDescriptorHeaps.pop();
    } else {
        descriptorHeap = CreateDescriptorHeap();
        m_DescriptorHeapPool.push(descriptorHeap);
    }

    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap() {
    auto device = static_cast<D3D12GraphicsManager*>(g_GraphicsManager.get())->m_Device;

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type                       = m_Type;
    descriptorHeapDesc.NumDescriptors             = m_NumDescriptorsPerHeap;
    descriptorHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(
    ID3D12GraphicsCommandList5*                                                        cmdList,
    std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc) {
    uint32_t numDescriptorsToCommit = StaleDescriptorCount();

    if (numDescriptorsToCommit <= 0) return;

    auto device = static_cast<D3D12GraphicsManager*>(g_GraphicsManager.get())->m_Device;
    assert(cmdList != nullptr);

    if (!m_CurrentDescriptorHeap || m_NumFreeHandles < numDescriptorsToCommit) {
        m_CurrentDescriptorHeap      = RequestDescriptorHeap();
        m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_NumFreeHandles             = m_NumDescriptorsPerHeap;

        m_CommandContext.SetDescriptorHeap(m_Type, m_CurrentDescriptorHeap.Get());
        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    DWORD rootIndex;
    while (_BitScanForward(&rootIndex, m_StaleDescriptorTableBitMask)) {
        UINT                         numSrcDesriptors     = m_DescriptorTableCache[rootIndex].numDescriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorHandles = m_DescriptorTableCache[rootIndex].baseHandle;

        D3D12_CPU_DESCRIPTOR_HANDLE start[] = {m_CurrentCPUDescriptorHandle};
        UINT                        size[]  = {numSrcDesriptors};

        device->CopyDescriptors(1, start, size, numSrcDesriptors, srcDescriptorHandles, nullptr, m_Type);

        // Set the descriptors on the command list using the passed-in setter function.
        setFunc(cmdList, rootIndex, m_CurrentGPUDescriptorHandle);

        m_CurrentCPUDescriptorHandle.Offset(numSrcDesriptors, m_HandleIncrementSize);
        m_CurrentGPUDescriptorHandle.Offset(numSrcDesriptors, m_HandleIncrementSize);
        m_NumFreeHandles -= numSrcDesriptors;

        // Filp the current bit
        m_StaleDescriptorTableBitMask ^= (1 << rootIndex);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(ID3D12GraphicsCommandList5* cmdList,
                                                                  D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle) {
    if (!m_CurrentDescriptorHeap || m_NumFreeHandles < 1) {
        m_CurrentDescriptorHeap      = RequestDescriptorHeap();
        m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_NumFreeHandles             = m_NumDescriptorsPerHeap;

        m_CommandContext.SetDescriptorHeap(m_Type, m_CurrentDescriptorHeap.Get());
        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    auto                        device = static_cast<D3D12GraphicsManager*>(g_GraphicsManager.get())->m_Device;
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_CurrentGPUDescriptorHandle;
    device->CopyDescriptorsSimple(1, m_CurrentCPUDescriptorHandle, cpuDescriptorHandle, m_Type);
    m_CurrentCPUDescriptorHandle.Offset(1, m_HandleIncrementSize);
    m_CurrentGPUDescriptorHandle.Offset(1, m_HandleIncrementSize);
    m_NumFreeHandles -= 1;

    return handle;
}

}  // namespace Hitagi::Graphics