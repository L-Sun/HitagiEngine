#include "DynamicDescriptorHeap.hpp"

#include "D3DCore.hpp"
#include "CommandContext.hpp"

namespace Hitagi::Graphics::DX12 {

std::mutex DynamicDescriptorHeap::kMutex;

DynamicDescriptorHeap::DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type)
    : m_Type(type),
      m_DescriptorTableBitMask(0),
      m_StaleDescriptorTableBitMask(0),
      m_CurrentCPUDescriptorHandle(D3D12_DEFAULT),
      m_CurrentGPUDescriptorHandle(D3D12_DEFAULT),
      m_NumFreeHandles(0),
      m_HandleIncrementSize(g_Device->GetDescriptorHandleIncrementSize(type)),
      m_DescriptorHandleCache(kNumDescriptorsPerHeap) {}

void DynamicDescriptorHeap::Reset(uint64_t fenceValue) {
    if (m_CurrentDescriptorHeap) {
        kAvailableDescriptorHeaps[m_Type].push({m_CurrentDescriptorHeap, m_FenceValue});
    }
    m_CurrentDescriptorHeap.Reset();
    m_CurrentCPUDescriptorHandle  = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_CurrentGPUDescriptorHandle  = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_NumFreeHandles              = 0;
    m_DescriptorTableBitMask      = 0;
    m_StaleDescriptorTableBitMask = 0;
    m_FenceValue                  = 0;

    // Reset the table cache
    for (int i = 0; i < kMaxDescriptorTables; ++i) m_DescriptorTableCache[i].Reset();
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature) {
    m_StaleDescriptorTableBitMask   = 0;
    const auto& rootSignatureDesc   = rootSignature.GetRootSignatureDesc();
    m_DescriptorTableBitMask        = rootSignature.GetDescriptorTableBitMask(m_Type);
    uint32_t descriptorTableBitMask = m_DescriptorTableBitMask;

    uint32_t currentOffset = 0;
    DWORD    rootIndex;
    while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters) {
        uint32_t numDescriptors = rootSignature.GetNumDescriptorsInTable(rootIndex);

        auto& tableCache          = m_DescriptorTableCache[rootIndex];
        tableCache.baseHandle     = &m_DescriptorHandleCache[currentOffset];
        tableCache.numDescriptors = numDescriptors;

        currentOffset += numDescriptors;
        // filp current bit
        descriptorTableBitMask ^= (1 << rootIndex);
    }
    assert(currentOffset <= kNumDescriptorsPerHeap &&
           "The root signature requires more than the maximum number of descriptors per descriptor "
           "heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptor(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors,
                                            const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorHandle) {
    if (numDescriptors > kNumDescriptorsPerHeap || rootParameterIndex >= kMaxDescriptorTables) throw std::bad_alloc();

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

std::pair<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, uint64_t> DynamicDescriptorHeap::RequestDescriptorHeap() {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    auto&                                        availableHeapPool = kAvailableDescriptorHeaps[m_Type];
    uint64_t                                     fenceValue        = 0;

    std::lock_guard lock(kMutex);
    if (!availableHeapPool.empty() && g_CommandManager.IsFenceComplete(availableHeapPool.front().second)) {
        descriptorHeap = availableHeapPool.front().first;
        fenceValue     = availableHeapPool.front().second;
        availableHeapPool.pop();
    } else {
        descriptorHeap = CreateDescriptorHeap(m_Type);
        kDescriptorHeapPool[m_Type].push(descriptorHeap);
    }

    return {descriptorHeap, fenceValue};
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE type) {
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type                       = type;
    descriptorHeapDesc.NumDescriptors             = kNumDescriptorsPerHeap;
    descriptorHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(g_Device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(
    CommandContext&                                                                    context,
    std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc) {
    uint32_t numDescriptorsToCommit = StaleDescriptorCount();
    auto     cmdList                = context.GetCommandList();

    if (numDescriptorsToCommit <= 0) return;

    auto device = g_Device;
    assert(cmdList != nullptr);

    if (!m_CurrentDescriptorHeap || m_NumFreeHandles < numDescriptorsToCommit) {
        auto heap                    = RequestDescriptorHeap();
        m_CurrentDescriptorHeap      = heap.first;
        m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_NumFreeHandles             = kNumDescriptorsPerHeap;
        m_FenceValue                 = heap.second;

        context.SetDescriptorHeap(m_Type, m_CurrentDescriptorHeap.Get());
        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    DWORD rootIndex;
    while (_BitScanForward(&rootIndex, m_StaleDescriptorTableBitMask)) {
        UINT                         numSrcDesriptors     = m_DescriptorTableCache[rootIndex].numDescriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorHandles = m_DescriptorTableCache[rootIndex].baseHandle;

        std::array start = {m_CurrentCPUDescriptorHandle};
        std::array size  = {numSrcDesriptors};

        if (srcDescriptorHandles->ptr != 0) {
            device->CopyDescriptors(1, start.data(), size.data(), numSrcDesriptors, srcDescriptorHandles, nullptr, m_Type);

            // Set the descriptors on the command list using the passed-in setter function.
            setFunc(cmdList, rootIndex, m_CurrentGPUDescriptorHandle);

            m_CurrentCPUDescriptorHandle.Offset(numSrcDesriptors, m_HandleIncrementSize);
            m_CurrentGPUDescriptorHandle.Offset(numSrcDesriptors, m_HandleIncrementSize);
            m_NumFreeHandles -= numSrcDesriptors;
        }

        // Filp the current bit
        m_StaleDescriptorTableBitMask ^= (1 << rootIndex);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(CommandContext&             context,
                                                                  D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle) {
    assert(cpuDescriptorHandle.ptr != 0);
    if (!m_CurrentDescriptorHeap || m_NumFreeHandles < 1) {
        auto heap                    = RequestDescriptorHeap();
        m_CurrentDescriptorHeap      = heap.first;
        m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_NumFreeHandles             = kNumDescriptorsPerHeap;
        m_FenceValue                 = heap.second;

        context.SetDescriptorHeap(m_Type, m_CurrentDescriptorHeap.Get());
        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_CurrentGPUDescriptorHandle;
    g_Device->CopyDescriptorsSimple(1, m_CurrentCPUDescriptorHandle, cpuDescriptorHandle, m_Type);
    m_CurrentCPUDescriptorHandle.Offset(1, m_HandleIncrementSize);
    m_CurrentGPUDescriptorHandle.Offset(1, m_HandleIncrementSize);
    m_NumFreeHandles -= 1;

    return handle;
}

}  // namespace Hitagi::Graphics::DX12