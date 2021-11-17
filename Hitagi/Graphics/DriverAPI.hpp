#pragma once
#include "Resource.hpp"
#include "Format.hpp"
#include "PipelineState.hpp"
#include "ICommandContext.hpp"

#include <vector>

namespace Hitagi::Graphics {
enum struct APIType {
    DirectX12,
    Vulkan,
    Melta,
    OpenGL
};

class DriverAPI {
public:
    virtual ~DriverAPI()                     = default;
    virtual void Present(size_t frame_index) = 0;

    // Buffer
    virtual void                            CreateSwapChain(uint32_t width, uint32_t height, unsigned frame_count, Format format, void* window)                       = 0;
    virtual std::shared_ptr<RenderTarget>   CreateRenderTarget(std::string_view name, const RenderTarget::Description& desc)                                          = 0;
    virtual std::shared_ptr<RenderTarget>   CreateRenderFromSwapChain(size_t frame_index)                                                                             = 0;
    virtual std::shared_ptr<VertexBuffer>   CreateVertexBuffer(std::string_view name, size_t vertex_count, size_t vertex_size, const uint8_t* initial_data = nullptr) = 0;
    virtual std::shared_ptr<IndexBuffer>    CreateIndexBuffer(std::string_view name, size_t index_count, size_t index_size, const uint8_t* initial_data = nullptr)    = 0;
    virtual std::shared_ptr<ConstantBuffer> CreateConstantBuffer(std::string_view name, size_t num_elements, size_t element_size)                                     = 0;
    virtual std::shared_ptr<TextureBuffer>  CreateTextureBuffer(std::string_view name, const TextureBuffer::Description& desc)                                        = 0;
    virtual std::shared_ptr<DepthBuffer>    CreateDepthBuffer(std::string_view name, const DepthBuffer::Description& desc)                                            = 0;

    virtual std::shared_ptr<Resource> GetSwapChainBuffer(size_t frame_index) = 0;

    virtual void UpdateConstantBuffer(std::shared_ptr<ConstantBuffer> buffer, size_t offset, const uint8_t* src, size_t size) = 0;
    virtual void ResizeConstantBuffer(std::shared_ptr<ConstantBuffer> buffer, size_t new_num_elements)                        = 0;

    // resource will relase after fence value and the ref count is zero!
    virtual void RetireResources(std::vector<std::shared_ptr<Resource>> resources, uint64_t fence_value) = 0;
    // Sampler
    virtual std::shared_ptr<Sampler> CreateSampler(std::string_view name, const Graphics::Sampler::Description& desc) = 0;

    // Pipeline
    virtual std::unique_ptr<backend::Resource> CreateRootSignature(const RootSignature& rootsignature) = 0;
    virtual std::unique_ptr<backend::Resource> CreatePipelineState(const PipelineState& pso)           = 0;

    // CommandList
    virtual std::shared_ptr<IGraphicsCommandContext> GetGraphicsCommandContext() = 0;

    // Fence
    virtual void WaitFence(uint64_t fence_value) = 0;
    virtual void IdleGPU()                       = 0;

    APIType GetType() const { return m_Type; }

    virtual void Test(RenderTarget& rt, const PipelineState& pso) = 0;

protected:
    DriverAPI(APIType type) : m_Type(type) {}
    const APIType m_Type;
};

}  // namespace Hitagi::Graphics