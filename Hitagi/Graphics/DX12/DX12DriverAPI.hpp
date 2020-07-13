#pragma once
#include "DriverAPI.hpp"
#include "Format.hpp"

#include "CommandContext.hpp"
#include "CommandListManager.hpp"
#include "DescriptorAllocator.hpp"
#include "RootSignature.hpp"
#include "PSO.hpp"

namespace Hitagi::Graphics::backend::DX12 {

class DX12DriverAPI : public DriverAPI {
public:
    DX12DriverAPI();
    ~DX12DriverAPI();

    void Present(size_t frameIndex) final;

    void                     CreateSwapChain(uint32_t width, uint32_t height, unsigned frameCount, Format format, void* window) final;
    Graphics::RenderTarget   CreateRenderTarget(std::string_view name, const Graphics::RenderTarget::Description& desc) final;
    Graphics::RenderTarget   CreateRenderFromSwapChain(size_t frameIndex) final;
    Graphics::VertexBuffer   CreateVertexBuffer(size_t vertexCount, size_t vertexSize, const uint8_t* initialData = nullptr) final;
    Graphics::IndexBuffer    CreateIndexBuffer(size_t indexCount, size_t indexSize, const uint8_t* initialData = nullptr) final;
    Graphics::ConstantBuffer CreateConstantBuffer(std::string_view name, size_t numElements, size_t elementSize) final;
    Graphics::TextureBuffer  CreateTextureBuffer(std::string_view name, const Graphics::TextureBuffer::Description& desc) final;
    Graphics::DepthBuffer    CreateDepthBuffer(std::string_view name, const Graphics::DepthBuffer::Description& desc) final;

    void UpdateConstantBuffer(Graphics::ConstantBuffer& buffer, size_t offset, const uint8_t* data, size_t size) final;

    void RetireResources(std::vector<Graphics::ResourceContainer>&& resources, uint64_t fenceValue) final;

    virtual Graphics::TextureSampler CreateSampler(std::string_view name, const Graphics::TextureSampler::Description& desc) final;

    void CreateRootSignature(const Graphics::RootSignature& signature) final;
    void DeleteRootSignature(const Graphics::RootSignature& signature) final;
    void CreatePipelineState(const Graphics::PipelineState& pso) final;
    void DeletePipelineState(const Graphics::PipelineState& pso) final;

    std::shared_ptr<IGraphicsCommandContext> GetGraphicsCommandContext() final;

    // Fence
    void WaitFence(uint64_t fenceValue) final;
    void IdleGPU() final;

    void test(Graphics::RenderTarget& rt, const Graphics::PipelineState& pso) final;

    static void ReportDebugLog();

    ID3D12Device*       GetDevice() const noexcept { return m_Device.Get(); }
    CommandListManager& GetCmdMgr() noexcept { return m_CommandManager; }
    GraphicsPSO&        GetPSO(const Graphics::PipelineState& pipeline) { return m_Pso.at(pipeline.GetName()); }
    RootSignature&      GetRootSignature(size_t id) { return m_RootSignatures.at(id); }

private:
    // Static method
    static DXGI_FORMAT ToDxgiFormat(Graphics::Format format) noexcept { return static_cast<DXGI_FORMAT>(format); }

    // No-static method
    // RootSignature& GetRootSignature();

    static ComPtr<ID3D12DebugDevice1> sm_DebugInterface;

    ComPtr<ID3D12Device6> m_Device;
    ComPtr<IDXGIFactory4> m_DxgiFactory;

    ComPtr<IDXGISwapChain4> m_SwapChain;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;

    CommandListManager                 m_CommandManager;
    std::array<DescriptorAllocator, 4> m_DescriptorAllocator = {
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_RTV},
        DescriptorAllocator{D3D12_DESCRIPTOR_HEAP_TYPE_DSV}};

    std::unordered_map<size_t, RootSignature>    m_RootSignatures;
    std::unordered_map<std::string, GraphicsPSO> m_Pso;

    // resource will release after fence complete
    std::queue<std::pair<uint64_t, std::vector<Graphics::ResourceContainer>>> m_RetireResources;
};
}  // namespace Hitagi::Graphics::backend::DX12
