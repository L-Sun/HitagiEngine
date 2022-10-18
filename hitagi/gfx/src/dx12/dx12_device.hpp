#pragma once
#include "descriptor_heap.hpp"
#include "dx12_resource.hpp"
#include "dx12_command_queue.hpp"
#include "d3dx12.h"

#include <hitagi/utils/utils.hpp>
#include <hitagi/gfx/device.hpp>

#include <spdlog/logger.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <deque>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device final : public Device {
public:
    DX12Device(std::string_view name);
    ~DX12Device() final;

    void WaitIdle() final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue* final;
    auto CreateCommandQueue(CommandType type, std::string_view name) -> std::shared_ptr<CommandQueue> final;
    auto CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> final;
    auto CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> final;
    auto CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> final;

    auto CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GpuBuffer> final;
    auto CreateBufferView(GpuBufferView::Desc desc) -> std::shared_ptr<GpuBufferView> final;
    auto CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreateTextureView(TextureView::Desc desc) -> std::shared_ptr<TextureView> final;
    auto CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler> final;

    void CompileShader(Shader& shader) final;
    auto CreateRenderPipeline(RenderPipeline::Desc desc) -> std::shared_ptr<RenderPipeline> final;

    static void ReportDebugLog();

    inline auto GetLogger() const noexcept { return m_Logger; };
    inline auto GetDevice() const noexcept { return m_Device; }

    auto RequestDynamicDescriptors(std::size_t num, D3D12_DESCRIPTOR_HEAP_TYPE type) -> Descriptor;
    void RetireDynamicDescriptors(Descriptor&& descriptor, std::uint64_t fence_value);

    bool FenceFinished(std::uint64_t fence_value);

private:
    void IntegrateD3D12Logger();
    void UnregisterIntegratedD3D12Logger();

    ComPtr<IDXGIFactory6> m_Factory;
    ComPtr<ID3D12Device9> m_Device;
    CD3DX12FeatureSupport m_FeatureSupport;

    DWORD m_DebugCookie;

    ComPtr<IDxcUtils>     m_Utils;
    ComPtr<IDxcCompiler3> m_ShaderCompiler;

    utils::EnumArray<std::unique_ptr<DescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE> m_DescriptorAllocators;
    // Only cbv_uav_srv and sampler allocator
    std::array<std::unique_ptr<DescriptorAllocator>, 2> m_GpuDescriptorAllocators;
    std::deque<std::pair<Descriptor, std::uint64_t>>    m_ReiteredGpuDescriptors;

    std::pmr::unordered_map<void*, std::shared_ptr<DX12SwapChain>> m_SwapChains;

    utils::EnumArray<std::shared_ptr<DX12CommandQueue>, CommandType> m_CommandQueues;
};
}  // namespace hitagi::gfx