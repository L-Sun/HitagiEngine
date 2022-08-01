#pragma once
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/gpu_resource.hpp>
#include <hitagi/graphics/command_context.hpp>

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
    virtual void CreateSwapChain(std::uint32_t width, std::uint32_t height, unsigned frame_count, resource::Format format, void* window) = 0;
    // Will return current back buffer index
    virtual std::size_t ResizeSwapChain(std::uint32_t width, std::uint32_t height) = 0;

    virtual void InitRenderTarget(RenderTarget& rt)                                 = 0;
    virtual void InitRenderFromSwapChain(RenderTarget& rt, std::size_t frame_index) = 0;
    virtual void InitVertexBuffer(resource::VertexArray& vb)                        = 0;
    virtual void InitIndexBuffer(resource::IndexArray& ib)                          = 0;
    virtual void InitTexture(resource::Texture& tb)                                 = 0;
    virtual void InitConstantBuffer(ConstantBuffer& cb)                             = 0;
    virtual void InitDepthBuffer(DepthBuffer& db)                                   = 0;
    virtual void InitSampler(Sampler& sampler)                                      = 0;
    virtual void InitPipelineState(PipelineState& pipeline)                         = 0;

    virtual void UpdateConstantBuffer(ConstantBuffer& buffer, std::size_t offset, const std::byte* src, std::size_t size) = 0;
    virtual void ResizeConstantBuffer(ConstantBuffer& buffer, std::size_t new_num_elements)                               = 0;

    // CommandList
    virtual std::shared_ptr<IGraphicsCommandContext> CreateGraphicsCommandContext() = 0;

    virtual void RetireResource(std::shared_ptr<backend::Resource> resource) = 0;

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