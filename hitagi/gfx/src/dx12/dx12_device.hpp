#pragma once
#include "dx12_command_queue.hpp"
#include "dx12_descriptor_heap.hpp"
#include "dx12_bindless.hpp"

#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/shader_compiler.hpp>

#include <d3dx12/d3dx12.h>
#include <D3D12MemAlloc.h>

#include <dxgi1_6.h>

namespace hitagi::gfx {
class DX12Device final : public Device {
public:
    DX12Device(std::string_view name);
    ~DX12Device() final;

    void Tick() final;

    void WaitIdle() final;

    auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> final;

    auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> final;
    auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> final;

    auto GetBindlessUtils() -> BindlessUtils& final;

    inline auto  GetFactory() const noexcept { return m_Factory; }
    inline auto  GetAdapter() const noexcept { return m_Adapter; }
    inline auto  GetDevice() const noexcept { return m_Device; }
    inline auto  GetAllocator() const noexcept { return m_MemoryAllocator; }
    inline auto& GetRTVDescriptorAllocator() const noexcept { return *m_RTVDescriptorAllocator; }
    inline auto& GetDSVDescriptorAllocator() const noexcept { return *m_DSVDescriptorAllocator; }

private:
    static void ReportDebugLog(const ComPtr<ID3D12Device>& device);
    void        Profile() const;

    void IntegrateD3D12Logger();
    void UnregisterIntegratedD3D12Logger();

    ComPtr<IDXGIFactory2> m_Factory;
    ComPtr<IDXGIAdapter4> m_Adapter;
    ComPtr<ID3D12Device>  m_Device;

    D3D12MA::ALLOCATION_CALLBACKS                                       m_CustomAllocationCallback;
    std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>> m_CustomAllocationInfos;
    ComPtr<D3D12MA::Allocator>                                          m_MemoryAllocator;

    DWORD m_DebugCookie;

    utils::EnumArray<std::shared_ptr<DX12CommandQueue>, CommandType> m_CommandQueues;

    std::unique_ptr<DescriptorAllocator> m_RTVDescriptorAllocator;
    std::unique_ptr<DescriptorAllocator> m_DSVDescriptorAllocator;

    std::unique_ptr<DX12BindlessUtils> m_BindlessUtils;
};
}  // namespace hitagi::gfx