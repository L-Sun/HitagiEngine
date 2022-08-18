#pragma once
#include "command_list_manager.hpp"
#include "descriptor_allocator.hpp"

#include <hitagi/graphics/device_api.hpp>

namespace hitagi::graphics::backend::DX12 {

class DX12Device : public DeviceAPI {
public:
    DX12Device();
    ~DX12Device() override;

    void        CreateSwapChain(std::uint32_t width, std::uint32_t height, unsigned frame_count, resource::Format format, void* window) final;
    std::size_t ResizeSwapChain(std::uint32_t width, std::uint32_t height) final;
    void        Present() final;

    void InitRenderTarget(graphics::RenderTarget& rt) final;
    void InitRenderFromSwapChain(graphics::RenderTarget& rt, std::size_t frame_index) final;
    void InitVertexBuffer(resource::VertexArray& vb) final;
    void InitIndexBuffer(resource::IndexArray& ib) final;
    void InitTexture(resource::Texture& tb) final;
    void InitConstantBuffer(graphics::ConstantBuffer& cb) final;
    void InitDepthBuffer(graphics::DepthBuffer& db) final;
    void InitSampler(graphics::Sampler& sampler) final;
    void InitPipelineState(graphics::PipelineState& pipeline) final;

    std::shared_ptr<IGraphicsCommandContext> CreateGraphicsCommandContext() final;

    void RetireResource(std::unique_ptr<backend::Resource> resource) final;

    // Fence
    bool IsFenceComplete(std::uint64_t fence_value) final;
    void WaitFence(std::uint64_t fence_value) final;
    void IdleGPU() final;

    static void ReportDebugLog();

    ID3D12Device*       GetDevice() const noexcept { return m_Device.Get(); }
    CommandListManager& GetCmdMgr() noexcept { return m_CommandManager; }

    DescriptorAllocator& GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type);

private:
    void                                       CompileShader(const std::shared_ptr<resource::Shader>& shader);
    std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(const std::shared_ptr<resource::Shader>& vs);

    void RetireResources();

    static ComPtr<ID3D12DebugDevice1> sm_DebugInterface;

    std::shared_ptr<spdlog::logger> m_Logger;

    ComPtr<ID3D12Device6> m_Device;
    ComPtr<IDXGIFactory4> m_DxgiFactory;

    ComPtr<IDxcUtils>     m_Utils;
    ComPtr<IDxcCompiler3> m_ShaderCompiler;

    ComPtr<IDXGISwapChain4> m_SwapChain;

    CommandListManager                 m_CommandManager;
    std::array<DescriptorAllocator, 4> m_DescriptorAllocator = {
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_RTV},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_DSV}};

    // resource will release after fence complete
    constexpr static auto retire_resource_cmp = [](const std::unique_ptr<backend::Resource>& r1, const std::unique_ptr<backend::Resource>& r2) {
        return r1->fence_value < r2->fence_value;
    };
    std::priority_queue<std::unique_ptr<backend::Resource>, std::pmr::vector<std::unique_ptr<backend::Resource>>, decltype(retire_resource_cmp)> m_RetiredResources;
};
}  // namespace hitagi::graphics::backend::DX12
