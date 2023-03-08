#pragma once
#include "descriptor_heap.hpp"
#include "dx12_resource.hpp"
#include "dx12_command_queue.hpp"
#include "d3dx12.h"

#include <hitagi/utils/utils.hpp>
#include <hitagi/gfx/device.hpp>

#include <spdlog/logger.h>
#include <D3D12MemAlloc.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <deque>
#include <memory>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device final : public Device {
public:
    DX12Device(std::string_view name);
    ~DX12Device() final;

    void WaitIdle() final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> final;
    auto CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> final;
    auto CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> final;

    auto CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGpuBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GpuBuffer> final;
    auto CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler> final;

    void CompileShader(Shader& shader) final;
    auto CreateRenderPipeline(RenderPipeline::Desc desc) -> std::shared_ptr<RenderPipeline> final;

    void Profile(std::size_t frame_index) const final;

    static void ReportDebugLog();

    inline auto GetLogger() const noexcept { return m_Logger; };
    inline auto GetDevice() const noexcept { return m_Device; }

    auto AllocateDescriptors(std::size_t num, D3D12_DESCRIPTOR_HEAP_TYPE type) -> Descriptor;
    auto AllocateDynamicDescriptors(std::size_t num, D3D12_DESCRIPTOR_HEAP_TYPE type) -> Descriptor;

private:
    void IntegrateD3D12Logger();
    void UnregisterIntegratedD3D12Logger();
    auto CreateInputLayout(Shader& vs) -> InputLayout;

    ComPtr<IDXGIFactory6> m_Factory;
    ComPtr<IDXGIAdapter4> m_Adapter;
    ComPtr<ID3D12Device9> m_Device;

    D3D12MA::ALLOCATION_CALLBACKS                                       m_CustomAllocationCallback;
    std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>> m_CustomAllocationInfos;
    ComPtr<D3D12MA::Allocator>                                          m_MemoryAllocator;

    CD3DX12FeatureSupport m_FeatureSupport;

    DWORD m_DebugCookie;

    ComPtr<IDxcUtils>     m_Utils;
    ComPtr<IDxcCompiler3> m_ShaderCompiler;

    utils::EnumArray<std::unique_ptr<DescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE> m_DescriptorAllocators;
    // Only cbv_uav_srv and sampler allocator
    std::array<std::unique_ptr<DescriptorAllocator>, 2> m_GpuDescriptorAllocators;

    utils::EnumArray<std::shared_ptr<DX12CommandQueue>, CommandType> m_CommandQueues;
};
}  // namespace hitagi::gfx