#pragma once
#include "command_context.hpp"
#include "command_list_manager.hpp"
#include "descriptor_allocator.hpp"
#include "root_signature.hpp"
#include "pso.hpp"

#include <hitagi/graphics/driver_api.hpp>

namespace hitagi::graphics::backend::DX12 {

class DX12DriverAPI : public DriverAPI {
public:
    DX12DriverAPI();
    ~DX12DriverAPI() override;

    void Present(size_t frame_index) final;

    void                                      CreateSwapChain(uint32_t width, uint32_t height, unsigned frame_count, Format format, void* window) final;
    std::shared_ptr<graphics::RenderTarget>   CreateRenderTarget(std::string_view name, const graphics::RenderTarget::Description& desc) final;
    std::shared_ptr<graphics::RenderTarget>   CreateRenderFromSwapChain(size_t frame_index) final;
    std::shared_ptr<graphics::VertexBuffer>   CreateVertexBuffer(std::string_view name, size_t vertex_count, size_t vertex_size, const uint8_t* initial_data = nullptr) final;
    std::shared_ptr<graphics::IndexBuffer>    CreateIndexBuffer(std::string_view name, size_t index_count, size_t index_size, const uint8_t* initial_data = nullptr) final;
    std::shared_ptr<graphics::ConstantBuffer> CreateConstantBuffer(std::string_view name, size_t num_elements, size_t element_size) final;
    std::shared_ptr<graphics::TextureBuffer>  CreateTextureBuffer(std::string_view name, const graphics::TextureBuffer::Description& desc) final;
    std::shared_ptr<graphics::DepthBuffer>    CreateDepthBuffer(std::string_view name, const graphics::DepthBuffer::Description& desc) final;

    size_t ResizeSwapChain(uint32_t width, uint32_t height) final;

    void UpdateConstantBuffer(std::shared_ptr<graphics::ConstantBuffer> buffer, size_t offset, const uint8_t* data, size_t size) final;
    void ResizeConstantBuffer(std::shared_ptr<graphics::ConstantBuffer> buffer, size_t new_num_elements) final;

    void RetireResources(std::vector<std::shared_ptr<graphics::Resource>> resources, uint64_t fence_value) final;

    std::shared_ptr<graphics::Sampler> CreateSampler(std::string_view name, const graphics::Sampler::Description& desc) final;

    std::unique_ptr<backend::Resource> CreateRootSignature(const graphics::RootSignature& rootsignature) final;
    std::unique_ptr<backend::Resource> CreatePipelineState(const graphics::PipelineState& pso) final;

    std::shared_ptr<IGraphicsCommandContext> GetGraphicsCommandContext() final;

    // Fence
    void WaitFence(uint64_t fence_value) final;
    void IdleGPU() final;

    void Test(graphics::RenderTarget& rt, const graphics::PipelineState& pso) final;

    static void ReportDebugLog();

    ID3D12Device*       GetDevice() const noexcept { return m_Device.Get(); }
    CommandListManager& GetCmdMgr() noexcept { return m_CommandManager; }

private:
    static ComPtr<ID3D12DebugDevice1> sm_DebugInterface;

    ComPtr<ID3D12Device6> m_Device;
    ComPtr<IDXGIFactory4> m_DxgiFactory;

    ComPtr<IDXGISwapChain4> m_SwapChain;

    CommandListManager                 m_CommandManager;
    std::array<DescriptorAllocator, 4> m_DescriptorAllocator = {
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_RTV},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_DSV}};

    // resource will release after fence complete
    std::queue<std::pair<uint64_t, std::vector<std::shared_ptr<graphics::Resource>>>> m_RetireResources;
};
}  // namespace hitagi::graphics::backend::DX12
