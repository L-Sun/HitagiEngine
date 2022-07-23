#pragma once
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/resource.hpp>
#include <hitagi/graphics/command_context.hpp>
#include <hitagi/resource/mesh.hpp>

#include <vector>

namespace hitagi::graphics {
enum struct APIType {
    DirectX12,
    Vulkan,
    Melta,
    OpenGL
};

class DeviceAPI {
public:
    virtual ~DeviceAPI()                     = default;
    virtual void Present(size_t frame_index) = 0;

    // Buffer
    virtual void                            CreateSwapChain(std::uint32_t width, std::uint32_t height, unsigned frame_count, Format format, void* window) = 0;
    virtual std::shared_ptr<RenderTarget>   CreateRenderTarget(std::string_view name, const RenderTargetDesc& desc)                                       = 0;
    virtual std::shared_ptr<RenderTarget>   CreateRenderFromSwapChain(size_t frame_index)                                                                 = 0;
    virtual std::shared_ptr<VertexBuffer>   CreateVertexBuffer(std::shared_ptr<resource::VertexArray> vertices)                                           = 0;
    virtual std::shared_ptr<IndexBuffer>    CreateIndexBuffer(std::shared_ptr<resource::IndexArray> indices)                                              = 0;
    virtual std::shared_ptr<TextureBuffer>  CreateTextureBuffer(std::shared_ptr<resource::Texture>)                                                       = 0;
    virtual std::shared_ptr<ConstantBuffer> CreateConstantBuffer(std::string_view name, ConstantBufferDesc)                                               = 0;
    virtual std::shared_ptr<DepthBuffer>    CreateDepthBuffer(std::string_view name, const DepthBufferDesc& desc)                                         = 0;

    // Temp buffer, used by frame graph and so on.
    virtual std::shared_ptr<TextureBuffer> CreateTextureBuffer(std::string_view name, TextureBufferDesc desc) = 0;

    // Will return current back buffer index
    virtual size_t ResizeSwapChain(std::uint32_t width, std::uint32_t height) = 0;

    virtual void UpdateConstantBuffer(std::shared_ptr<ConstantBuffer> buffer, size_t offset, const std::byte* src, size_t size) = 0;
    virtual void ResizeConstantBuffer(std::shared_ptr<ConstantBuffer> buffer, size_t new_num_elements)                          = 0;

    // resource will relase after fence value and the ref count is zero!
    virtual void RetireResource(std::shared_ptr<Resource> resource, std::uint64_t fence_value) = 0;
    // Sampler
    virtual std::shared_ptr<Sampler> CreateSampler(std::string_view name, const resource::SamplerDesc& desc) = 0;

    // Pipeline
    virtual std::shared_ptr<PipelineState> CreatePipelineState(std::string_view name, const PipelineStateDesc& desc) = 0;

    // CommandList
    virtual std::shared_ptr<IGraphicsCommandContext> GetGraphicsCommandContext() = 0;

    // Fence
    virtual bool IsFenceComplete(std::uint64_t fence_value) = 0;
    virtual void WaitFence(std::uint64_t fence_value)       = 0;
    virtual void IdleGPU()                                  = 0;

    APIType GetType() const { return m_Type; }

protected:
    DeviceAPI(APIType type) : m_Type(type) {}
    const APIType m_Type;
};

}  // namespace hitagi::graphics