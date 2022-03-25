#include "dynamic_descriptor_heap.hpp"
#include "command_context.hpp"

namespace hitagi::graphics::backend::DX12 {

std::mutex DynamicDescriptorHeap::sm_Mutex;

DynamicDescriptorHeap::DynamicDescriptorHeap(ID3D12Device*              device,
                                             D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             FenceChecker&&             checker)
    : m_Device(device),
      m_Type(type),
      m_DescriptorTableBitMask(0),
      m_StaleDescriptorTableBitMask(0),
      m_CurrentCPUDescriptorHandle(D3D12_DEFAULT),
      m_CurrentGPUDescriptorHandle(D3D12_DEFAULT),
      m_NumFreeHandles(0),
      m_HandleIncrementSize(device->GetDescriptorHandleIncrementSize(type)),
      m_FenceChecker(std::move(checker)),
      m_DescriptorHandleCache(sm_NumDescriptorsPerHeap) {}

void DynamicDescriptorHeap::Reset(FenceValue fence_value) {
    if (m_CurrentDescriptorHeap) {
        sm_AvailableDescriptorHeaps[m_Type].push({m_CurrentDescriptorHeap, fence_value});
    }
    m_CurrentDescriptorHeap.Reset();
    m_CurrentCPUDescriptorHandle  = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_CurrentGPUDescriptorHandle  = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    m_NumFreeHandles              = 0;
    m_DescriptorTableBitMask      = 0;
    m_StaleDescriptorTableBitMask = 0;

    // Reset the table cache
    for (int i = 0; i < sm_MaxDescriptorTables; ++i) m_DescriptorTableCache[i].Reset();
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& root_signature) {
    m_StaleDescriptorTableBitMask      = 0;
    const auto& root_signature_desc    = root_signature.GetRootSignatureDesc();
    m_DescriptorTableBitMask           = root_signature.GetDescriptorTableBitMask(m_Type);
    uint32_t descriptor_table_bit_mask = m_DescriptorTableBitMask;

    uint32_t current_offset = 0;
    DWORD    root_index     = 0;
    while (_BitScanForward(&root_index, descriptor_table_bit_mask) && root_index < root_signature_desc.NumParameters) {
        uint32_t num_descriptors = root_signature.GetNumDescriptorsInTable(root_index);

        auto& table_cache           = m_DescriptorTableCache[root_index];
        table_cache.base_handle     = &m_DescriptorHandleCache[current_offset];
        table_cache.num_descriptors = num_descriptors;

        current_offset += num_descriptors;
        // filp current bit
        descriptor_table_bit_mask ^= (1 << root_index);
    }
    assert(current_offset <= sm_NumDescriptorsPerHeap &&
           "The root signature requires more than the maximum number of descriptors per descriptor "
           "heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptors(uint32_t root_parameter_index, uint32_t offset, const std::vector<Descriptor>& descriptors) {
    if (descriptors.size() > sm_NumDescriptorsPerHeap || root_parameter_index >= sm_MaxDescriptorTables) throw std::bad_alloc();

    auto& table_cache = m_DescriptorTableCache[root_parameter_index];
    if (offset + descriptors.size() > table_cache.num_descriptors)
        throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");

    D3D12_CPU_DESCRIPTOR_HANDLE* handle = table_cache.base_handle + offset;
    for (uint32_t i = 0; i < descriptors.size(); i++) {
        handle[i] = descriptors[i].handle;
    }
    m_StaleDescriptorTableBitMask |= (1 << root_parameter_index);
}

uint32_t DynamicDescriptorHeap::StaleDescriptorCount() const {
    uint32_t num_stale_descriptors      = 0;
    DWORD    i                          = 0;
    DWORD    stale_descriptors_bit_mask = m_StaleDescriptorTableBitMask;
    while (_BitScanForward(&i, stale_descriptors_bit_mask)) {
        num_stale_descriptors += m_DescriptorTableCache[i].num_descriptors;
        stale_descriptors_bit_mask ^= (1 << i);
    }

    return num_stale_descriptors;
}

std::pair<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, uint64_t> DynamicDescriptorHeap::RequestDescriptorHeap() {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    auto&                                        available_heap_pool = sm_AvailableDescriptorHeaps[m_Type];
    uint64_t                                     fence_value         = 0;

    std::lock_guard lock(sm_Mutex);
    if (!available_heap_pool.empty() && m_FenceChecker(available_heap_pool.front().second)) {
        descriptor_heap = available_heap_pool.front().first;
        fence_value     = available_heap_pool.front().second;
        available_heap_pool.pop();
    } else {
        descriptor_heap = CreateDescriptorHeap(m_Device, m_Type);
        sm_DescriptorHeapPool[m_Type].push(descriptor_heap);
    }

    return {descriptor_heap, fence_value};
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap(
    ID3D12Device*              device,
    D3D12_DESCRIPTOR_HEAP_TYPE type) {
    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
    descriptor_heap_desc.Type                       = type;
    descriptor_heap_desc.NumDescriptors             = sm_NumDescriptorsPerHeap;
    descriptor_heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    ThrowIfFailed(device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));

    return descriptor_heap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(
    CommandContext&                                                                    context,
    std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> set_func) {
    uint32_t num_descriptors_to_commit = StaleDescriptorCount();
    auto     cmd_list                  = context.GetCommandList();

    if (num_descriptors_to_commit <= 0) return;

    assert(cmd_list != nullptr);

    if (!m_CurrentDescriptorHeap || m_NumFreeHandles < num_descriptors_to_commit) {
        auto heap                    = RequestDescriptorHeap();
        m_CurrentDescriptorHeap      = heap.first;
        m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_NumFreeHandles             = sm_NumDescriptorsPerHeap;

        context.SetDescriptorHeap(m_Type, m_CurrentDescriptorHeap.Get());
        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    DWORD root_index = 0;
    while (_BitScanForward(&root_index, m_StaleDescriptorTableBitMask)) {
        UINT                         num_src_desriptors     = m_DescriptorTableCache[root_index].num_descriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* src_descriptor_handles = m_DescriptorTableCache[root_index].base_handle;

        std::array start = {m_CurrentCPUDescriptorHandle};
        std::array size  = {num_src_desriptors};

        if (src_descriptor_handles->ptr != 0) {
            m_Device->CopyDescriptors(1, start.data(), size.data(), num_src_desriptors, src_descriptor_handles, nullptr, m_Type);

            // Set the descriptors on the command list using the passed-in setter function.
            set_func(cmd_list, root_index, m_CurrentGPUDescriptorHandle);

            m_CurrentCPUDescriptorHandle.Offset(num_src_desriptors, m_HandleIncrementSize);
            m_CurrentGPUDescriptorHandle.Offset(num_src_desriptors, m_HandleIncrementSize);
            m_NumFreeHandles -= num_src_desriptors;
        }

        // Filp the current bit
        m_StaleDescriptorTableBitMask ^= (1 << root_index);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(CommandContext&             context,
                                                                  D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle) {
    assert(cpu_descriptor_handle.ptr != 0);
    if (!m_CurrentDescriptorHeap || m_NumFreeHandles < 1) {
        auto heap                    = RequestDescriptorHeap();
        m_CurrentDescriptorHeap      = heap.first;
        m_CurrentCPUDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_CurrentGPUDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_NumFreeHandles             = sm_NumDescriptorsPerHeap;

        context.SetDescriptorHeap(m_Type, m_CurrentDescriptorHeap.Get());
        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_CurrentGPUDescriptorHandle;
    m_Device->CopyDescriptorsSimple(1, m_CurrentCPUDescriptorHandle, cpu_descriptor_handle, m_Type);
    m_CurrentCPUDescriptorHandle.Offset(1, m_HandleIncrementSize);
    m_CurrentGPUDescriptorHandle.Offset(1, m_HandleIncrementSize);
    m_NumFreeHandles -= 1;

    return handle;
}

}  // namespace hitagi::graphics::backend::DX12